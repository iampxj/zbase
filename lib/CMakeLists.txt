
zephyr_library_sources(
    ${CMAKE_CURRENT_SOURCE_DIR}/iovprintf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/printer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/timer/timer_list.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libitoa.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libassert.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libstring.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libcrc16.c
    ${CMAKE_CURRENT_SOURCE_DIR}/libcrc32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/fnmatch.c
)

if (NOT CONFIG_BOOTLOADER)
    add_subdirectory(cfgfile)
    zephyr_library_sources(
        ${CMAKE_CURRENT_SOURCE_DIR}/msg_storage.c
        ${CMAKE_CURRENT_SOURCE_DIR}/msg_partition_backend.c
    )
    if (CONFIG_FTRACE)
        zephyr_library_sources(
            ${CMAKE_CURRENT_SOURCE_DIR}/callpath.c
        )
    endif()
endif()

zephyr_library_sources_ifdef(CONFIG_MSG_FILE_BACKEND
    ${CMAKE_CURRENT_SOURCE_DIR}/msg_file_backend.c
)

zephyr_library_sources_ifdef(CONFIG_COMPRESS_LZ4
    ${CMAKE_CURRENT_SOURCE_DIR}/lz4.c
)

zephyr_library_sources_ifndef(CONFIG_BOOTLOADER
    ${CMAKE_CURRENT_SOURCE_DIR}/disklog.c
)

if (NOT WINDOWS)
    zephyr_library_sources(
        ${CMAKE_CURRENT_SOURCE_DIR}/bitops.c
    )
endif()
