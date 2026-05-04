add_library(cppflux
    schedulers.cpp
    server.cpp
    router.cpp
    response.cpp
    thread_pool.cpp
    request.cpp
    timer.cpp
    logger.cpp
)

# BUILD_INTERFACE: path used when building from source.
# INSTALL_INTERFACE: path used by consumers after cmake --install.
target_include_directories(cppflux
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${LIBEVENT_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}   # for io_thread.hpp and other private headers
)

target_link_libraries(cppflux
    PUBLIC ${LIBEVENT_LIBRARIES} pthread
)

install(TARGETS cppflux
    EXPORT      CppfluxTargets
    ARCHIVE     DESTINATION lib
    LIBRARY     DESTINATION lib
    RUNTIME     DESTINATION bin
)

target_compile_options(cppflux PRIVATE
    ${LIBEVENT_CFLAGS_OTHER}
    $<$<BOOL:${NATIVE_ARCH}>:-march=native>
    $<$<CONFIG:Release>:-O3 -flto=auto>
    $<$<CONFIG:RelWithDebInfo>:-O2>
)
