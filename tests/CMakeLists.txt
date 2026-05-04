include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
set(BUILD_GMOCK    OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST  OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(cppflux_tests
    test_thread_pool.cpp
    test_task.cpp
    test_router.cpp
)

target_link_libraries(cppflux_tests PRIVATE cppflux GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(cppflux_tests)
