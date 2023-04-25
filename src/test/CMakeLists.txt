cmake_minimum_required(VERSION 3.17)
project(nimble_ball C)
enable_testing()

set(CMAKE_C_STANDARD 99)


set(deps ../../deps/)
set(local_deps ./deps/)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(isDebug TRUE)
else()
    set(isDebug FALSE)
endif()

if (isDebug)
    message("Debug build detected")
    add_compile_definitions(CONFIGURATION_DEBUG TYRAN_CONFIGURATION_DEBUG)
    set(CONFIGURATION_DEBUG 1)
endif()

file(GLOB_RECURSE local_deps_src FOLLOW_SYMLINKS
        "${local_deps}*/src/lib/*.c"
        "${deps}*/*/src/platform/posix/*.c"
        "${deps}*/*/src/lib/*.c"
        )

add_executable(nimble_ball_test
    main.c
    test.c
    ${local_deps_src}
)

add_test(NAME nimble_ball_test
         COMMAND nimble_ball_test)


target_include_directories(nimble_ball_test PUBLIC ${deps}piot/imprint/src/include)
target_include_directories(nimble_ball_test PUBLIC ${deps}piot/clog/src/include)
target_include_directories(nimble_ball_test PUBLIC ${deps}piot/tiny-libc/src/include)
target_include_directories(nimble_ball_test PUBLIC ${deps}piot/flood-c/src/include)
target_include_directories(nimble_ball_test PUBLIC ${DEPS}piot/nimble-steps-serialize-c/src/include)

if (WIN32)
target_link_libraries(nimble_ball_test nimble_ball)
else()
target_link_libraries(nimble_ball_test nimble_ball m)
endif(WIN32)

