cmake_minimum_required(VERSION 3.16)
project(TrajectorySystem LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/utf-8)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Natural-language control is separate from the legacy demonstration entry point.
add_library(firmware_decoder STATIC firmware/protocol.c)
target_compile_definitions(firmware_decoder PRIVATE MCU_HOST_TEST)
target_include_directories(firmware_decoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/firmware)
add_library(motion_control STATIC
    src/control/plan.cpp src/control/ai.cpp src/control/executor.cpp
    src/control/device.cpp src/control/preview.cpp)
target_include_directories(motion_control PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/control ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(motion_control PRIVATE firmware_decoder)
if(WIN32)
    target_link_libraries(motion_control PRIVATE winhttp)
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/docs/natural-language-plan.example.json)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/docs/natural-language-plan.example.json MOTION_DEMO_JSON)
configure_file(src/control/demo.h.in generated/demo.h @ONLY)
add_executable(MotionControl src/control/main.cpp)
target_include_directories(MotionControl PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(MotionControl PRIVATE motion_control)
if(MINGW)
    target_link_options(MotionControl PRIVATE -municode)
endif()

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
