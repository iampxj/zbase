

target_sources(basework
    PRIVATE
    osapi.c
    osapi_fs.c
    osapi_obj.c
)

if (NOT CONFIG_BOOTLOADER)
target_sources(basework
    PRIVATE
    osapi_bus.c
)
endif()

if (LINUX)
    add_subdirectory(posix)
elseif (WINDOWS)
    add_subdirectory(win32)
elseif (THREADX_ARCH)
    add_subdirectory(threadx)
else()
    add_subdirectory(zephyr)
endif()
