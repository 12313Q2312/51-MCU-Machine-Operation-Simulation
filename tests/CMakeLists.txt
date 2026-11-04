add_executable(test_motion test_motion.cpp ../src/motion.cpp)
target_include_directories(test_motion PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME test_motion COMMAND test_motion)

add_executable(test_protocol test_protocol.cpp ../src/protocol.cpp)
target_include_directories(test_protocol PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
add_test(NAME test_protocol COMMAND test_protocol)

# Both tests use assert; keep those checks active in Release builds too.
foreach(test_target IN ITEMS test_motion test_protocol)
    if(MSVC)
        target_compile_options(${test_target} PRIVATE /UNDEBUG)
    else()
        target_compile_options(${test_target} PRIVATE -UNDEBUG)
    endif()
endforeach()

add_library(firmware_led_host STATIC ../firmware/led_matrix.c)
target_compile_definitions(firmware_led_host PUBLIC MCU_HOST_TEST)
target_include_directories(firmware_led_host PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../firmware)
add_executable(test_motion_control test_motion_control.cpp)
target_include_directories(test_motion_control PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../generated)
target_link_libraries(test_motion_control PRIVATE motion_control firmware_decoder firmware_led_host)
add_test(NAME test_motion_control COMMAND test_motion_control)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME test_control_cli COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/test_control_cli.py $<TARGET_FILE:MotionControl>
    ${CMAKE_CURRENT_SOURCE_DIR}/../docs/natural-language-plan.example.json)
