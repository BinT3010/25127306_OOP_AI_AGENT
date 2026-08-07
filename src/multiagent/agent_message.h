#pragma once
/**
 * @file agent_message.h
 * @brief Định dạng thông điệp trao đổi giữa các sub-agent (mục 10.3).
 */
#include <string>

namespace agent::multiagent {

struct AgentMessage {
    std::string from_agent_id;
    std::string type;     ///< "step" | "result" | "error"
    std::string payload;  ///< nội dung tự do (thường là tóm tắt ngắn 1 dòng)
};

}  // namespace agent::multiagent
