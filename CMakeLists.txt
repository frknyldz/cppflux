cmake_minimum_required(VERSION 3.20)
project(cppflux CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBEVENT REQUIRED libevent libevent_pthreads)
find_package(glog REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CCACHE_BIN ccache)
if(CCACHE_BIN)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_BIN})
endif()

add_executable(server
    src/main.cpp
    src/schedulers.cpp
    src/thread_pool.cpp
    src/response.cpp
    src/router.cpp
    src/server.cpp
)

target_include_directories(server PRIVATE ${LIBEVENT_INCLUDE_DIRS} src)
target_link_libraries(server PRIVATE ${LIBEVENT_LIBRARIES} pthread glog::glog)
option(NATIVE_ARCH "Optimize for the host CPU architecture" OFF)

target_compile_options(server PRIVATE ${LIBEVENT_CFLAGS_OTHER}
    $<$<BOOL:${NATIVE_ARCH}>:-march=native>
    $<$<CONFIG:Release>:-O3 -flto=auto>
    $<$<CONFIG:RelWithDebInfo>:-O2>
)
target_link_options(server PRIVATE
    $<$<BOOL:${NATIVE_ARCH}>:-fuse-ld=mold>
    $<$<CONFIG:Release>:-flto=auto>
)
