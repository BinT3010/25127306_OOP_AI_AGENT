#pragma once
/**
 * @file action_parser.h
 * @brief Phân tích (parse) ChatResult thô từ LLMClient thành một Action (mục
 * 3.4: "Parse tool call từ LLM response (regex hoặc JSON parsing)").
 *
 * Ưu tiên hai nguồn, theo thứ tự:
 *   1) Native tool_calls — nếu OllamaClient đã trích xuất được structured
 *      tool_calls từ model hỗ trợ function-calling gốc (vd: qwen3, llama3.1+).
 *   2) Text theo định dạng ReAct (Thought/Action/Action Input/Final Answer),
 *      trích xuất bằng std::regex — dùng cho model không hỗ trợ tool-calling
 *      gốc hoặc khi model trả lời thuần văn bản.
 */
#include "../client/llm_types.h"
#include "action.h"

namespace agent {

[[nodiscard]] Action parse_action(const ChatResult& result);

}  // namespace agent
