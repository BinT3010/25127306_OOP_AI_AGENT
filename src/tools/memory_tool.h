#pragma once
/**
 * @file memory_tool.h
 * @brief Tool "memory" — tool bắt buộc #5/5, gộp memory_save + memory_search.
 * Backing store: SQLite (libsqlite3). Hỗ trợ mở rộng BONUS "Persistent Memory
 * với Vector Search" (mục 10.2 đề bài, +4đ): nếu được cung cấp một
 * EmbeddingFn (thường trỏ tới LLMClient::embed của model nomic-embed-text
 * qua Ollama), memory_search sẽ xếp hạng theo cosine similarity thay vì
 * khớp chuỗi con (LIKE) — độ chính xác ngữ nghĩa cao hơn nhiều.
 */
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "tool.h"
#include "../util/exceptions.h"

struct sqlite3;  // forward declare kiểu C opaque, tránh include sqlite3.h trong header public

namespace agent {

/// std::span (C++20): view không sở hữu dữ liệu, cho phép cosine_similarity
/// nhận BẤT KỲ dãy float liên tục nào (vector, array, hay con trỏ+kích thước)
/// mà không ép người gọi phải có sẵn std::vector — tổng quát hơn tham chiếu
/// const vector&, đúng tinh thần dùng span khi hàm chỉ ĐỌC dữ liệu liên tục.
double cosine_similarity(std::span<const float> a, std::span<const float> b);

class MemoryTool : public Tool {
public:
    using EmbeddingFn = std::function<std::expected<std::vector<float>, std::string>(const std::string&)>;

    explicit MemoryTool(std::filesystem::path db_path, EmbeddingFn embedding_fn = nullptr);

    [[nodiscard]] std::string name() const override { return "memory"; }
    [[nodiscard]] std::string description() const override {
        return "Lưu trữ dài hạn xuyên suốt các lượt chạy. action=memory_save cần 'key' và "
               "'content'. action=memory_search cần 'query' (và top_k tuỳ chọn, mặc định 3); "
               "nếu có embedding model sẽ tìm theo ngữ nghĩa (vector similarity), nếu không sẽ "
               "tìm theo khớp chuỗi con.";
    }
    [[nodiscard]] std::string parameters_schema() const override {
        return R"({"action": "memory_save|memory_search", "key": string (save), )"
               R"("content": string (save), "query": string (search), "top_k": int (search, default 3)})";
    }
    [[nodiscard]] bool is_mutating() const override { return true; }
    [[nodiscard]] ToolResult execute(const std::string& args_json, Environment& env) override;

    [[nodiscard]] bool vector_search_enabled() const noexcept { return static_cast<bool>(embedding_fn_); }

private:
    struct Sqlite3Closer {
        void operator()(sqlite3* db) const;
    };
    std::unique_ptr<sqlite3, Sqlite3Closer> db_;
    EmbeddingFn embedding_fn_;

    void ensure_schema();
    [[nodiscard]] ToolResult do_save(const std::string& key, const std::string& content);
    [[nodiscard]] ToolResult do_search(const std::string& query, int top_k);
};

}  // namespace agent
