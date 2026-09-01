/**
 * @file test_util.cpp
 * @brief Unit test cho tầng util: Registry<T>, exceptions, LoopDetector.
 */
#include "doctest.h"

#include "../src/agent/loop_detector.h"
#include "../src/util/exceptions.h"
#include "../src/util/registry.h"

namespace {
struct IAnimal {
    virtual ~IAnimal() = default;
    virtual std::string sound() const = 0;
};
struct Dog : IAnimal {
    std::string sound() const override { return "gau gau"; }
};
struct Cat : IAnimal {
    std::string sound() const override { return "meo meo"; }
};
}  // namespace

TEST_CASE("Registry<T>: đăng ký, tạo, liệt kê") {
    agent::util::Registry<IAnimal> reg;
    CHECK(reg.size() == 0);

    reg.register_type("dog", [] { return std::make_unique<Dog>(); });
    reg.register_type("cat", [] { return std::make_unique<Cat>(); });

    CHECK(reg.size() == 2);
    CHECK(reg.has("dog"));
    CHECK_FALSE(reg.has("bird"));

    auto d = reg.create("dog");
    REQUIRE(d != nullptr);
    CHECK(d->sound() == "gau gau");

    CHECK(reg.create("bird") == nullptr);

    auto names = reg.names();
    REQUIRE(names.size() == 2);
    CHECK(names[0] == "cat");  // đã sort
    CHECK(names[1] == "dog");
}

TEST_CASE("Exception hierarchy: mọi lớp con đều bắt được qua AgentException&") {
    CHECK_THROWS_AS(throw agent::ToolNotFoundException("foo"), agent::AgentException);
    CHECK_THROWS_AS(throw agent::ConfigException("bad config"), agent::AgentException);
    CHECK_THROWS_AS(throw agent::EnvironmentViolationException("escape"), agent::AgentException);

    try {
        throw agent::ToolNotFoundException("calculator_v2");
    } catch (const agent::AgentException& e) {
        std::string msg = e.what();
        CHECK(msg.find("calculator_v2") != std::string::npos);
    }
}

TEST_CASE("LoopDetector: phát hiện lặp lại y hệt (generic repeat)") {
    agent::LoopDetector::Config cfg;
    cfg.repeat_warning_threshold = 2;
    cfg.repeat_critical_threshold = 4;
    agent::LoopDetector det(cfg);

    for (int i = 0; i < 3; ++i) {
        auto r = det.record_and_check("calc|{\"expression\":\"1+1\"}");
        if (i < 1) {
            CHECK(r.severity == agent::LoopSeverity::kNone);
        } else {
            CHECK(r.severity == agent::LoopSeverity::kWarning);
        }
    }
    auto r4 = det.record_and_check("calc|{\"expression\":\"1+1\"}");
    CHECK(r4.severity == agent::LoopSeverity::kCritical);
    CHECK(r4.should_abort());
}

TEST_CASE("LoopDetector: phát hiện ping-pong giữa 2 hành động") {
    agent::LoopDetector::Config cfg;
    cfg.pingpong_warning_cycles = 2;
    cfg.pingpong_critical_cycles = 3;
    agent::LoopDetector det(cfg);

    agent::LoopDetectionResult last;
    for (int i = 0; i < 6; ++i) {
        std::string sig = (i % 2 == 0) ? "toolA|{}" : "toolB|{}";
        last = det.record_and_check(sig);
    }
    CHECK(last.kind == agent::LoopKind::kPingPong);
    CHECK(last.should_abort());
}

TEST_CASE("LoopDetector: reset() xoá lịch sử, không còn phát hiện loop cũ") {
    agent::LoopDetector det;
    for (int i = 0; i < 5; ++i) det.record_and_check("x|{}");
    det.reset();
    auto r = det.record_and_check("x|{}");
    CHECK(r.severity == agent::LoopSeverity::kNone);
}

TEST_CASE("LoopDetector: hành động khác nhau xen kẽ KHÔNG bị coi là loop") {
    agent::LoopDetector det;
    auto r1 = det.record_and_check("a|{}");
    auto r2 = det.record_and_check("b|{}");
    auto r3 = det.record_and_check("c|{}");
    CHECK(r1.severity == agent::LoopSeverity::kNone);
    CHECK(r2.severity == agent::LoopSeverity::kNone);
    CHECK(r3.severity == agent::LoopSeverity::kNone);
}
