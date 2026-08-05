/**
 * @file skill_loader.cpp
 * @see skill_loader.h
 */
#include "skill_loader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include "../util/exceptions.h"

namespace agent {

namespace {
std::string trim(const std::string& s) {
    std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> split_csv(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        std::string t = trim(item);
        if (!t.empty()) out.push_back(t);
    }
    return out;
}
}  // namespace

SkillLoader::SkillLoader(std::filesystem::path skills_dir) : skills_dir_(std::move(skills_dir)) {
    reload();
}

void SkillLoader::reload() {
    skills_.clear();
    if (!std::filesystem::exists(skills_dir_)) {
        throw ConfigException("Thư mục skills không tồn tại: " + skills_dir_.string());
    }
    // Range-based for + std::filesystem::directory_iterator (mục V, "std::filesystem").
    for (const auto& entry : std::filesystem::directory_iterator(skills_dir_)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".md") continue;
        skills_.push_back(parse_skill_file(entry.path()));
    }
    std::ranges::sort(skills_, {}, &Skill::name);  // thứ tự ổn định, tiện log/test
}

Skill SkillLoader::parse_skill_file(const std::filesystem::path& path) {
    std::ifstream ifs(path);
    if (!ifs) throw SkillLoadException(path.string(), "không mở được file");

    std::string first_line;
    std::getline(ifs, first_line);
    if (trim(first_line) != "---") {
        throw SkillLoadException(path.string(),
                                  "thiếu front-matter mở đầu bằng '---' (dòng đầu tiên phải là '---')");
    }

    Skill skill;
    skill.source_path = path;
    std::string line;
    bool closed = false;
    while (std::getline(ifs, line)) {
        if (trim(line) == "---") {
            closed = true;
            break;
        }
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;  // bỏ qua dòng metadata không hiểu được
        std::string key = trim(line.substr(0, colon));
        std::string value = trim(line.substr(colon + 1));
        if (key == "name") {
            skill.name = value;
        } else if (key == "keywords") {
            skill.keywords = split_csv(value);
        }
    }
    if (!closed) {
        throw SkillLoadException(path.string(), "thiếu '---' đóng front-matter");
    }
    if (skill.name.empty()) {
        throw SkillLoadException(path.string(), "front-matter thiếu trường bắt buộc 'name'");
    }

    std::ostringstream content;
    content << ifs.rdbuf();
    skill.content = trim(content.str());
    return skill;
}

std::string SkillLoader::normalize(const std::string& s) {
    std::string out = s;
    // Chuẩn hoá chữ hoa/thường cho phần ASCII. Ký tự tiếng Việt có dấu (đa byte
    // UTF-8) được giữ nguyên: task/skill trong dự án dùng nhất quán cùng một
    // cách gõ dấu nên so khớp trực tiếp vẫn hoạt động tốt trong thực tế; xử lý
    // "bỏ dấu" đầy đủ (accent folding) được ghi nhận là hướng cải tiến trong
    // báo cáo, không bắt buộc theo đề bài (chỉ yêu cầu "keyword matching").
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return (c < 128) ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
    });
    return out;
}

std::vector<const Skill*> SkillLoader::select_for_task(const std::string& task_description,
                                                        int max_skills) const {
    std::string norm_task = normalize(task_description);

    struct Scored {
        const Skill* skill;
        int score;
    };
    std::vector<Scored> scored;
    for (const auto& skill : skills_) {
        int score = 0;
        for (const auto& kw : skill.keywords) {
            if (norm_task.find(normalize(kw)) != std::string::npos) ++score;
        }
        if (score > 0) scored.push_back({&skill, score});
    }
    std::ranges::stable_sort(scored, [](const Scored& a, const Scored& b) { return a.score > b.score; });

    std::vector<const Skill*> result;
    for (int i = 0; i < max_skills && i < static_cast<int>(scored.size()); ++i) {
        result.push_back(scored[static_cast<std::size_t>(i)].skill);
    }
    return result;
}

std::string SkillLoader::render_injection_block(const std::vector<const Skill*>& selected) {
    if (selected.empty()) return "";
    std::ostringstream oss;
    oss << "# Hướng dẫn kỹ năng liên quan (Skills)\n\n";
    for (const Skill* s : selected) {
        oss << "## " << s->name << "\n" << s->content << "\n\n";
    }
    return oss.str();
}

}  // namespace agent
