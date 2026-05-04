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

# PUBLIC: any target that links cppflux automatically gets include/cppflux/*.
# PRIVATE: libevent headers are an implementation detail — not leaked to users.
target_include_directories(cppflux
    PUBLIC  ${CMAKE_SOURCE_DIR}/include
    PRIVATE ${LIBEVENT_INCLUDE_DIRS}
            ${CMAKE_CURRENT_SOURCE_DIR}   # for io_thread.hpp and other private headers
)

target_link_libraries(cppflux
    PUBLIC ${LIBEVENT_LIBRARIES} pthread
)

target_compile_options(cppflux PRIVATE
    ${LIBEVENT_CFLAGS_OTHER}
    $<$<BOOL:${NATIVE_ARCH}>:-march=native>
    $<$<CONFIG:Release>:-O3 -flto=auto>
    $<$<CONFIG:RelWithDebInfo>:-O2>
)
