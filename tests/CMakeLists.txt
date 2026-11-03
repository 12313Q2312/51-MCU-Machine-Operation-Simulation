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
