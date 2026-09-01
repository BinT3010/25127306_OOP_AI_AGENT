#include "calculator_tool.h"

#include <cmath>
#include <expected>
#include <nlohmann/json.hpp>
#include <sstream>

namespace agent {

namespace {

/// Recursive-descent parser cho biểu thức số học.
/// Văn phạm (grammar), theo thứ tự ưu tiên tăng dần:
///   expr   := term (('+' | '-') term)*
///   term   := power (('*' | '/') power)*
///   power  := unary ('^' power)?              // luỹ thừa kết hợp phải (right-assoc)
///   unary  := ('-' | '+')? atom
///   atom   := NUMBER | '(' expr ')'
class ExprParser {
public:
    explicit ExprParser(std::string_view text) : text_(text) {}

    std::expected<double, std::string> parse() {
        skip_ws();
        auto v = parse_expr();
        if (!v) return v;
        skip_ws();
        if (pos_ != text_.size()) {
            return std::unexpected("Dư ký tự không hợp lệ tại vị trí " + std::to_string(pos_) + ": '" +
                                    std::string(text_.substr(pos_)) + "'");
        }
        return v;
    }

private:
    std::string_view text_;
    std::size_t pos_ = 0;

    void skip_ws() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
    }
    char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }

    std::expected<double, std::string> parse_expr() {
        auto lhs = parse_term();
        if (!lhs) return lhs;
        double result = *lhs;
        skip_ws();
        while (peek() == '+' || peek() == '-') {
            char op = peek();
            ++pos_;
            auto rhs = parse_term();
            if (!rhs) return rhs;
            result = (op == '+') ? result + *rhs : result - *rhs;
            skip_ws();
        }
        return result;
    }

    std::expected<double, std::string> parse_term() {
        auto lhs = parse_power();
        if (!lhs) return lhs;
        double result = *lhs;
        skip_ws();
        while (peek() == '*' || peek() == '/') {
            char op = peek();
            ++pos_;
            auto rhs = parse_power();
            if (!rhs) return rhs;
            if (op == '/') {
                if (*rhs == 0.0) return std::unexpected("Lỗi chia cho 0");
                result = result / *rhs;
            } else {
                result = result * *rhs;
            }
            skip_ws();
        }
        return result;
    }

    std::expected<double, std::string> parse_power() {
        auto base = parse_unary();
        if (!base) return base;
        skip_ws();
        if (peek() == '^') {
            ++pos_;
            auto exp = parse_power();  // kết hợp phải: 2^3^2 == 2^(3^2)
            if (!exp) return exp;
            return std::pow(*base, *exp);
        }
        return base;
    }

    std::expected<double, std::string> parse_unary() {
        skip_ws();
        if (peek() == '-') {
            ++pos_;
            auto v = parse_unary();
            if (!v) return v;
            return -(*v);
        }
        if (peek() == '+') {
            ++pos_;
            return parse_unary();
        }
        return parse_atom();
    }

    std::expected<double, std::string> parse_atom() {
        skip_ws();
        if (peek() == '(') {
            ++pos_;
            auto v = parse_expr();
            if (!v) return v;
            skip_ws();
            if (peek() != ')') return std::unexpected("Thiếu dấu ')' đóng ngoặc");
            ++pos_;
            return v;
        }
        std::size_t start = pos_;
        bool seen_digit_or_dot = false;
        while (pos_ < text_.size() &&
               (std::isdigit(static_cast<unsigned char>(text_[pos_])) || text_[pos_] == '.')) {
            seen_digit_or_dot = true;
            ++pos_;
        }
        if (!seen_digit_or_dot) {
            return std::unexpected("Kỳ vọng một con số tại vị trí " + std::to_string(pos_));
        }
        std::string num_str(text_.substr(start, pos_ - start));
        try {
            return std::stod(num_str);
        } catch (const std::exception&) {
            return std::unexpected("Không parse được số: '" + num_str + "'");
        }
    }
};

}  // namespace

ToolResult CalculatorTool::execute(const std::string& args_json, Environment& /*env*/) {
    std::string expr;
    try {
        auto j = nlohmann::json::parse(args_json);
        expr = j.at("expression").get<std::string>();
    } catch (const nlohmann::json::exception& e) {
        return ToolResult::fail(std::string("Tham số JSON không hợp lệ cho calculator: ") + e.what());
    }

    ExprParser parser(expr);
    auto result = parser.parse();
    if (!result) return ToolResult::fail(result.error());

    std::ostringstream oss;
    oss << *result;
    return ToolResult::ok(oss.str());
}

}  // namespace agent
