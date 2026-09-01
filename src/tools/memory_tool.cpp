/**
 * @file memory_tool.cpp
 * @see memory_tool.h
 */
#include "memory_tool.h"

#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <format>
#include <nlohmann/json.hpp>
#include <sstream>

#include "../util/exceptions.h"

namespace agent {

double cosine_similarity(std::span<const float> a, std::span<const float> b) {
    if (a.empty() || b.empty() || a.size() != b.size()) return -1.0;
    double dot = 0.0, norm_a = 0.0, norm_b = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot += static_cast<double>(a[i]) * b[i];
        norm_a += static_cast<double>(a[i]) * a[i];
        norm_b += static_cast<double>(b[i]) * b[i];
    }
    if (norm_a <= 0.0 || norm_b <= 0.0) return -1.0;
    return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

void MemoryTool::Sqlite3Closer::operator()(sqlite3* db) const {
    if (db) sqlite3_close(db);
}

namespace {
/// RAII cho sqlite3_stmt* — đảm bảo sqlite3_finalize luôn được gọi.
struct StmtGuard {
    sqlite3_stmt* stmt = nullptr;
    ~StmtGuard() {
        if (stmt) sqlite3_finalize(stmt);
    }
};
}  // namespace

MemoryTool::MemoryTool(std::filesystem::path db_path, EmbeddingFn embedding_fn)
    : embedding_fn_(std::move(embedding_fn)) {
    // SQLite tự tạo FILE nhưng không tự tạo THƯ MỤC cha — phải đảm bảo trước,
    // nếu không sqlite3_open sẽ thất bại với lỗi khó hiểu "unable to open
    // database file" khi db_path nằm trong một thư mục chưa tồn tại.
    if (!db_path.parent_path().empty()) {
        std::filesystem::create_directories(db_path.parent_path());
    }
    sqlite3* raw = nullptr;
    if (sqlite3_open(db_path.string().c_str(), &raw) != SQLITE_OK) {
        std::string msg = raw ? sqlite3_errmsg(raw) : "unknown error";
        if (raw) sqlite3_close(raw);
        throw ConfigException("Không mở được SQLite DB tại '" + db_path.string() + "': " + msg);
    }
    db_.reset(raw);
    ensure_schema();
}

void MemoryTool::ensure_schema() {
    const char* sql =
        "CREATE TABLE IF NOT EXISTS memory ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "key TEXT NOT NULL,"
        "content TEXT NOT NULL,"
        "embedding BLOB,"
        "created_at TEXT NOT NULL);";
    char* errmsg = nullptr;
    if (sqlite3_exec(db_.get(), sql, nullptr, nullptr, &errmsg) != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "unknown error";
        sqlite3_free(errmsg);
        throw ConfigException("Không tạo được bảng 'memory': " + msg);
    }
}

ToolResult MemoryTool::execute(const std::string& args_json, Environment& /*env*/) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(args_json);
    } catch (const nlohmann::json::exception& e) {
        return ToolResult::fail(std::string("Tham số JSON không hợp lệ cho memory: ") + e.what());
    }
    std::string action = j.value("action", "");

    if (action == "memory_save") {
        if (!j.contains("key") || !j.contains("content")) {
            return ToolResult::fail("memory_save cần cả 'key' và 'content'");
        }
        return do_save(j.at("key").get<std::string>(), j.at("content").get<std::string>());
    }
    if (action == "memory_search") {
        if (!j.contains("query")) return ToolResult::fail("memory_search cần 'query'");
        int top_k = j.value("top_k", 3);
        return do_search(j.at("query").get<std::string>(), top_k);
    }
    return ToolResult::fail("action không hợp lệ: '" + action + "' (chỉ chấp nhận memory_save|memory_search)");
}

ToolResult MemoryTool::do_save(const std::string& key, const std::string& content) {
    std::vector<float> emb;
    bool have_emb = false;
    if (embedding_fn_) {
        auto r = embedding_fn_(content);
        if (r.has_value()) {
            emb = *r;
            have_emb = true;
        }
        // Nếu embed lỗi (vd: Ollama tạm không phản hồi), vẫn lưu record không kèm
        // vector — memory_search sẽ tự fallback LIKE cho record này (an toàn, không mất dữ liệu).
    }

    const char* sql = "INSERT INTO memory (key, content, embedding, created_at) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* raw_stmt = nullptr;
    if (sqlite3_prepare_v2(db_.get(), sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        return ToolResult::fail(std::string("SQLite prepare lỗi: ") + sqlite3_errmsg(db_.get()));
    }
    StmtGuard guard{raw_stmt};

    sqlite3_bind_text(guard.stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(guard.stmt, 2, content.c_str(), -1, SQLITE_TRANSIENT);
    if (have_emb) {
        sqlite3_bind_blob(guard.stmt, 3, emb.data(), static_cast<int>(emb.size() * sizeof(float)),
                           SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(guard.stmt, 3);
    }
    auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    std::string ts = std::format("{:%Y-%m-%d %H:%M:%S}", now);
    sqlite3_bind_text(guard.stmt, 4, ts.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(guard.stmt) != SQLITE_DONE) {
        return ToolResult::fail(std::string("SQLite insert lỗi: ") + sqlite3_errmsg(db_.get()));
    }
    return ToolResult::ok("Đã lưu memory key='" + key + "'" + (have_emb ? " (kèm vector embedding)" : ""));
}

ToolResult MemoryTool::do_search(const std::string& query, int top_k) {
    if (top_k <= 0) top_k = 3;

    if (embedding_fn_) {
        auto qemb = embedding_fn_(query);
        if (qemb.has_value()) {
            const char* sql = "SELECT key, content, embedding FROM memory WHERE embedding IS NOT NULL;";
            sqlite3_stmt* raw_stmt = nullptr;
            if (sqlite3_prepare_v2(db_.get(), sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
                return ToolResult::fail(std::string("SQLite prepare lỗi: ") + sqlite3_errmsg(db_.get()));
            }
            StmtGuard guard{raw_stmt};

            struct Scored {
                std::string key, content;
                double score;
            };
            std::vector<Scored> scored;
            while (sqlite3_step(guard.stmt) == SQLITE_ROW) {
                std::string key = reinterpret_cast<const char*>(sqlite3_column_text(guard.stmt, 0));
                std::string content = reinterpret_cast<const char*>(sqlite3_column_text(guard.stmt, 1));
                const void* blob = sqlite3_column_blob(guard.stmt, 2);
                int nbytes = sqlite3_column_bytes(guard.stmt, 2);
                std::vector<float> emb(static_cast<std::size_t>(nbytes) / sizeof(float));
                if (nbytes > 0) std::memcpy(emb.data(), blob, static_cast<std::size_t>(nbytes));
                scored.push_back({std::move(key), std::move(content), cosine_similarity(*qemb, emb)});
            }
            std::ranges::sort(scored, [](const auto& a, const auto& b) { return a.score > b.score; });

            if (scored.empty()) return ToolResult::ok("Chưa có memory nào được lưu (kèm embedding).");
            std::ostringstream oss;
            int n = std::min<int>(top_k, static_cast<int>(scored.size()));
            for (int i = 0; i < n; ++i) {
                oss << (i + 1) << ". [" << scored[i].key << "] (similarity="
                    << std::format("{:.3f}", scored[i].score) << ") " << scored[i].content << "\n";
            }
            return ToolResult::ok(oss.str());
        }
        // Lỗi khi embed câu query (vd: Ollama tạm ngưng) -> rơi xuống fallback LIKE bên dưới.
    }

    const char* sql = "SELECT key, content FROM memory WHERE content LIKE ? OR key LIKE ? ORDER BY id DESC LIMIT ?;";
    sqlite3_stmt* raw_stmt = nullptr;
    if (sqlite3_prepare_v2(db_.get(), sql, -1, &raw_stmt, nullptr) != SQLITE_OK) {
        return ToolResult::fail(std::string("SQLite prepare lỗi: ") + sqlite3_errmsg(db_.get()));
    }
    StmtGuard guard{raw_stmt};
    std::string pattern = "%" + query + "%";
    sqlite3_bind_text(guard.stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(guard.stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(guard.stmt, 3, top_k);

    std::ostringstream oss;
    int count = 0;
    while (sqlite3_step(guard.stmt) == SQLITE_ROW) {
        std::string key = reinterpret_cast<const char*>(sqlite3_column_text(guard.stmt, 0));
        std::string content = reinterpret_cast<const char*>(sqlite3_column_text(guard.stmt, 1));
        oss << (++count) << ". [" << key << "] " << content << "\n";
    }
    if (count == 0) return ToolResult::ok("Không tìm thấy memory nào khớp truy vấn: " + query);
    return ToolResult::ok(oss.str());
}

}  // namespace agent
