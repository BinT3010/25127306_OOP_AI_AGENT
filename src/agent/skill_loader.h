#pragma once
/**
 * @file skill_loader.h
 * @brief Nạp "Skill" (hướng dẫn hành vi dạng Markdown) từ thư mục skills/ và
 * chọn skill phù hợp với một task cụ thể — mục 3.3 đề bài.
 *
 * Định dạng file skill (.md), lấy cảm hứng từ Claude Skills / OpenClaw SKILL.md:
 *
 *   ---
 *   name: task_planner
 *   keywords: kế hoạch, lập kế hoạch, plan, nhiều bước, multi-step
 *   ---
 *   # Nội dung hướng dẫn agent (được inject thẳng vào system prompt)
 *   ...
 *
 * Phần "front-matter" giữa hai dòng `---` là bắt buộc và chứa metadata dùng để
 * chọn skill (keyword matching). Phần còn lại của file là nội dung hướng dẫn
 * (system prompt injection). Dùng std::filesystem để quét thư mục — đúng yêu
 * cầu bảng V (mục "std::filesystem").
 */
#include <filesystem>
#include <string>
#include <vector>

#include "../util/exceptions.h"

namespace agent {

struct Skill {
    std::string name;
    std::vector<std::string> keywords;
    std::string content;
    std::filesystem::path source_path;
};

class SkillLoader {
public:
    /// Quét toàn bộ file *.md trong `skills_dir` (không đệ quy thư mục con) và
    /// nạp thành Skill. Ném SkillLoadException nếu front-matter thiếu 'name'.
    explicit SkillLoader(std::filesystem::path skills_dir);

    /// Nạp lại từ đĩa (hữu ích nếu thư mục skills/ thay đổi giữa các lần chạy).
    void reload();

    [[nodiscard]] const std::vector<Skill>& all() const noexcept { return skills_; }
    [[nodiscard]] std::size_t size() const noexcept { return skills_.size(); }

    /// Chọn (các) skill phù hợp nhất với `task_description` bằng keyword
    /// matching (đếm số từ khoá xuất hiện dạng substring, không phân biệt hoa
    /// thường / dấu). Trả về tối đa `max_skills` skill có điểm > 0, sắp xếp
    /// giảm dần theo điểm khớp; trả về rỗng nếu không skill nào khớp.
    [[nodiscard]] std::vector<const Skill*> select_for_task(const std::string& task_description,
                                                             int max_skills = 2) const;

    /// Ghép nội dung các skill đã chọn thành một khối văn bản, sẵn sàng nhúng
    /// vào system prompt (mục 3.3: "Skill được inject vào system prompt").
    [[nodiscard]] static std::string render_injection_block(const std::vector<const Skill*>& selected);

private:
    std::filesystem::path skills_dir_;
    std::vector<Skill> skills_;

    [[nodiscard]] static Skill parse_skill_file(const std::filesystem::path& path);
    [[nodiscard]] static std::string normalize(const std::string& s);
};

}  // namespace agent
