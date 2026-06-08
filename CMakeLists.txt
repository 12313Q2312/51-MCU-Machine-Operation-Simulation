cmake_minimum_required(VERSION 3.16)
project(TrajectorySystem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- PC 控制层 ----
add_executable(TrajectorySystem
    src/main.cpp
    src/motion.cpp
    src/AI.cpp
    src/protocol.cpp
    src/DSAI.cpp
    src/serial.cpp
)

target_include_directories(TrajectorySystem PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Windows: 链接 WinHTTP (DeepSeek API 需要)
if(WIN32)
    target_link_libraries(TrajectorySystem winhttp)
endif()

# 复制 trajectory.txt 到构建目录以便运行时读取
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/matlab/trajectory.txt
    ${CMAKE_CURRENT_BINARY_DIR}/trajectory.txt
    COPYONLY
)

# ---- 测试 ----
option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
