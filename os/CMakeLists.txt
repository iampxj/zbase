

target_sources(basework
    PRIVATE
    osapi.c
    osapi_fs.c
    osapi_obj.c
)

if (LINUX)
    add_subdirectory(posix)
elseif (WINDOWS)
    add_subdirectory(win32)
else()
    add_subdirectory(zephyr)
endif()
