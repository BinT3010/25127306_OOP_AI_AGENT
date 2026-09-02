#include "word_count_tool.h"
#include <nlohmann/json.hpp>
#include <utility>

namespace agent {

namespace {

// `std::istringstream >> word` (cách đếm cũ) chỉ nhận diện khoảng trắng ASCII
// (0x09-0x0D, 0x20). Nếu 2 từ trong văn bản bị nối bởi một ký tự khoảng trắng
// Unicode khác — phổ biến nhất là non-breaking space U+00A0 mà Microsoft Word
// hay tự động chèn (AutoCorrect, giữa số và đơn vị...), hoặc các khoảng trắng
// Unicode khác lỡ dính khi copy-paste — cách đếm cũ sẽ GỘP 2 từ đó thành 1
// token và đếm THIẾU. Hàm dưới đây coi mọi code point có thuộc tính Unicode
// White_Space là một dấu phân cách từ, không chỉ khoảng trắng ASCII.
bool is_unicode_space(char32_t cp) {
    switch (cp) {
        case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: case 0x20:  // ASCII whitespace
        case 0x85:                                                         // NEL
        case 0xA0:                                                         // NO-BREAK SPACE (hay gặp nhất)
        case 0x1680:                                                       // Ogham space mark
        case 0x2028: case 0x2029:                                          // line/paragraph separator
        case 0x202F:                                                       // narrow no-break space
        case 0x205F:                                                       // medium mathematical space
        case 0x3000:                                                       // ideographic space (CJK)
            return true;
        default:
            return cp >= 0x2000 && cp <= 0x200A;  // en quad .. hair space
    }
}

// Giải mã 1 code point UTF-8 bắt đầu tại text[i]. Trả về {code point, số byte đã
// tiêu thụ}. Nếu gặp chuỗi byte không hợp lệ (hiếm, vd file bị lỗi encoding) thì
// coi như 1 byte đơn để không bao giờ đọc lố ra ngoài chuỗi hay crash.
std::pair<char32_t, int> decode_utf8(const std::string& text, std::size_t i) {
    const auto b0 = static_cast<unsigned char>(text[i]);
    int len;
    char32_t cp;
    if ((b0 & 0x80) == 0x00) {
        return {b0, 1};
    } else if ((b0 & 0xE0) == 0xC0 && i + 1 < text.size()) {
        len = 2; cp = b0 & 0x1F;
    } else if ((b0 & 0xF0) == 0xE0 && i + 2 < text.size()) {
        len = 3; cp = b0 & 0x0F;
    } else if ((b0 & 0xF8) == 0xF0 && i + 3 < text.size()) {
        len = 4; cp = b0 & 0x07;
    } else {
        return {b0, 1};
    }
    for (int k = 1; k < len; ++k) {
        const auto bk = static_cast<unsigned char>(text[i + k]);
        if ((bk & 0xC0) != 0x80) return {b0, 1};  // continuation byte sai -> fallback an toàn
        cp = (cp << 6) | (bk & 0x3F);
    }
    return {cp, len};
}

int count_words_utf8(const std::string& text) {
    int count = 0;
    bool in_word = false;
    std::size_t i = 0;
    while (i < text.size()) {
        const auto [cp, len] = decode_utf8(text, i);
        if (is_unicode_space(cp)) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            ++count;
        }
        i += static_cast<std::size_t>(len);
    }
    return count;
}

}  // namespace

ToolResult WordCountTool::execute(const std::string& args_json, Environment& /*env*/) {
    std::string text;
    try {
        auto j = nlohmann::json::parse(args_json);
        text = j.at("text").get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        return ToolResult::fail(std::string("Tham số JSON không hợp lệ: ") + e.what());
    }

    return ToolResult::ok("Số từ: " + std::to_string(count_words_utf8(text)));
}

}  // namespace agent