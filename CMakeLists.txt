# CMakeLists.txt — OOP AI Agent Framework
#
# Build:
#   mkdir build && cd build
#   cmake .. -DCMAKE_CXX_COMPILER=g++-14        # chỉ định compiler hỗ trợ C++26 nếu g++ mặc định là bản cũ
#   make -j$(nproc)
#
# Sinh ra 3 executable trong build/bin/:
#   agent       — CLI chạy một agent task đơn lẻ (src/main.cpp)
#   run_eval    — chạy toàn bộ benchmark/tasks.json qua HarnessRunner
#   run_tests   — bộ unit test (doctest)
#
# Yêu cầu: g++-14 trở lên (hoặc Clang/MSVC tương đương hỗ trợ C++26 draft),
# libcurl-dev, libsqlite3-dev, nlohmann-json3-dev. Xem README.md.

cmake_minimum_required(VERSION 3.20)
project(OopAiAgentFramework CXX)

# ---------------------------------------------------------------------------
# Kiểm tra compiler có hỗ trợ -std=c++26 hay không NGAY khi configure, để báo
# lỗi rõ ràng thay vì lỗi cú pháp khó hiểu giữa chừng lúc build (mục đích sư
# phạm: sinh viên/giảng viên chạy `cmake ..` biết ngay cần cài g++-14+).
# ---------------------------------------------------------------------------
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++26" COMPILER_SUPPORTS_CXX26)
if(NOT COMPILER_SUPPORTS_CXX26)
    message(FATAL_ERROR
        "Compiler '${CMAKE_CXX_COMPILER}' không hỗ trợ -std=c++26.\n"
        "Dự án dùng một số kỹ thuật C++26 (vd: placeholder variables '_').\n"
        "Trên Ubuntu 24.04: sudo apt install g++-14 gcc-14, rồi chạy lại:\n"
        "  cmake .. -DCMAKE_CXX_COMPILER=g++-14")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# ---------------------------------------------------------------------------
# Dependencies bên ngoài
# ---------------------------------------------------------------------------
find_package(CURL REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

# Áp dụng -std=c++26 + cảnh báo cho MỌI target trong dự án (không dùng
# CMAKE_CXX_STANDARD vì một số phiên bản CMake chưa nhận diện "26" cho GNU —
# xem ghi chú trong báo cáo, mục "Build system").
function(agent_apply_common_settings target)
    target_compile_options(${target} PRIVATE -std=c++26 -Wall -Wextra -Wno-unused-parameter)
    target_compile_features(${target} PRIVATE cxx_std_23)  # sàn tối thiểu để CMake hiểu ý định
endfunction()

# ---------------------------------------------------------------------------
# Thư viện lõi: agent_core — toàn bộ logic dùng chung giữa agent/run_eval/tests
# ---------------------------------------------------------------------------
file(GLOB_RECURSE AGENT_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/agent/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/client/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/tools/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/harness/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/multiagent/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/util/*.cpp"
)

add_library(agent_core STATIC ${AGENT_CORE_SOURCES})
target_include_directories(agent_core PUBLIC ${CMAKE_SOURCE_DIR})
agent_apply_common_settings(agent_core)
target_link_libraries(agent_core PUBLIC
    CURL::libcurl
    SQLite::SQLite3
    nlohmann_json::nlohmann_json
    Threads::Threads
)

# ---------------------------------------------------------------------------
# Executable: agent (CLI demo — src/main.cpp)
# ---------------------------------------------------------------------------
add_executable(agent src/main.cpp)
target_link_libraries(agent PRIVATE agent_core)
agent_apply_common_settings(agent)

# ---------------------------------------------------------------------------
# Executable: run_eval (benchmark harness)
# ---------------------------------------------------------------------------
add_executable(run_eval benchmark/run_eval.cpp)
target_link_libraries(run_eval PRIVATE agent_core)
agent_apply_common_settings(run_eval)

# ---------------------------------------------------------------------------
# Executable: run_tests (doctest unit tests)
# ---------------------------------------------------------------------------
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.cpp")
add_executable(run_tests ${TEST_SOURCES})
target_include_directories(run_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
target_link_libraries(run_tests PRIVATE agent_core)
target_compile_definitions(run_tests PRIVATE PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")
agent_apply_common_settings(run_tests)

# ---------------------------------------------------------------------------
enable_testing()
add_test(NAME unit_tests COMMAND run_tests)

message(STATUS "=======================================================")
message(STATUS " OOP AI Agent Framework — cấu hình build hoàn tất")
message(STATUS "   Compiler : ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
message(STATUS "   Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "   make -j\$(nproc)  rồi xem build/bin/{agent,run_eval,run_tests}")
message(STATUS "=======================================================")
