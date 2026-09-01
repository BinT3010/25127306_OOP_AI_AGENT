#pragma once
/**
 * @file functional_evaluator.h
 * @brief Chấm điểm bằng cách CHẠY THẬT một shell script kiểm tra hiệu ứng phụ
 * trên hệ thống (Task::eval_script — mục 7.2), vd: kiểm tra file đã được tạo
 * đúng nội dung hay chưa. Đáng tin cậy hơn KeywordEvaluator cho các task có
 * yêu cầu "làm" (side-effect) thay vì chỉ "trả lời".
 */
#include <filesystem>

#include "evaluator.h"

namespace agent {

class FunctionalEvaluator : public Evaluator {
public:
    /// @param working_directory  thư mục chạy eval_script — PHẢI trùng với
    ///        working_directory của Environment mà agent đã thao tác, để
    ///        script kiểm tra đúng các file agent vừa tạo/sửa.
    explicit FunctionalEvaluator(std::filesystem::path working_directory, int timeout_seconds = 10)
        : working_directory_(std::move(working_directory)), timeout_seconds_(timeout_seconds) {}

    [[nodiscard]] EvalResult evaluate(const Trajectory& trajectory, const Task& task) override;
    [[nodiscard]] std::string name() const override { return "functional"; }

private:
    std::filesystem::path working_directory_;
    int timeout_seconds_;
};

}  // namespace agent
