zephyr_include_directories(.)

if (WINDOWS)
    set(CURRENT_TARGET ${TargetName})
else ()
    set(CURRENT_TARGET basework)
endif ()

target_sources(${CURRENT_TARGET}
    PRIVATE
    os_platform.cc
    os_fs_impl.c
    os_timer_impl.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/timer/timer_list.c
)