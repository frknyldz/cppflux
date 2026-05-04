add_executable(server main.cpp)

target_link_libraries(server PRIVATE cppflux)

target_compile_options(server PRIVATE
    $<$<BOOL:${NATIVE_ARCH}>:-march=native>
    $<$<CONFIG:Release>:-O3 -flto=auto>
    $<$<CONFIG:RelWithDebInfo>:-O2>
)
target_link_options(server PRIVATE
    $<$<BOOL:${NATIVE_ARCH}>:-fuse-ld=mold>
    $<$<CONFIG:Release>:-flto=auto>
)
