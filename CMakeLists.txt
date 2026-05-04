cmake_minimum_required(VERSION 3.20)
project(cppflux CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED libevent libevent_pthreads)

find_program(CCACHE_BIN ccache)
if(CCACHE_BIN)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_BIN})
endif()

option(NATIVE_ARCH            "Optimize for the host CPU architecture" ON)
option(CPPFLUX_BUILD_EXAMPLES "Build usage examples"                   ON)
option(CPPFLUX_BUILD_TESTS    "Build test suite"                        OFF)
option(BUILD_SHARED_LIBS      "Build cppflux as a shared library"      OFF)

# ── Library ───────────────────────────────────────────────────────────────────
add_subdirectory(lib)

# ── Examples (opt-out for library consumers via -DCPPFLUX_BUILD_EXAMPLES=OFF) ─
if(CPPFLUX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ── Tests (opt-in via -DCPPFLUX_BUILD_TESTS=ON) ───────────────────────────────
if(CPPFLUX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ── Linting ───────────────────────────────────────────────────────────────────
file(GLOB_RECURSE ALL_SOURCES
    ${CMAKE_SOURCE_DIR}/include/cppflux/*.hpp
    ${CMAKE_SOURCE_DIR}/lib/*.cpp
    ${CMAKE_SOURCE_DIR}/examples/*.cpp
)

find_program(CLANG_FORMAT_BIN clang-format)
if(CLANG_FORMAT_BIN)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_BIN} -i ${ALL_SOURCES}
        COMMENT "clang-format: formatting all sources"
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_BIN} --dry-run --Werror ${ALL_SOURCES}
        COMMENT "clang-format: checking formatting"
    )
endif()

find_program(CLANG_TIDY_BIN clang-tidy)
if(CLANG_TIDY_BIN)
    file(GLOB_RECURSE TIDY_SOURCES
        ${CMAKE_SOURCE_DIR}/lib/*.cpp
        ${CMAKE_SOURCE_DIR}/examples/*.cpp
    )
    add_custom_target(lint
        COMMAND ${CLANG_TIDY_BIN} -p ${CMAKE_BINARY_DIR} ${TIDY_SOURCES}
        COMMENT "clang-tidy: linting sources"
    )
endif()
