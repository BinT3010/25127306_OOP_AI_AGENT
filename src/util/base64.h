#pragma once
/**
 * @file base64.h
 * @brief Mã hoá base64 tối giản — dùng để nhúng ảnh nhị phân vào JSON request
 * gửi tới Ollama (trường "images", mục 3.1: hỗ trợ multimodal) và bởi
 * VLMEvaluator khi đính kèm ảnh bằng chứng.
 */
#include <cstdint>
#include <string>
#include <vector>

namespace agent::util {

[[nodiscard]] std::string base64_encode(const std::vector<std::uint8_t>& data);
[[nodiscard]] std::string base64_encode_file(const std::string& path);

}  // namespace agent::util
