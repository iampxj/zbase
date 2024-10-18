zephyr_include_directories(
    .
    ${CMAKE_CURRENT_SOURCE_DIR}/../../arch/x86
)

target_sources(basework
    PRIVATE
    os_platform.cc
    os_vflash_impl.cc
    os_timer_impl.c
    os_unix_impl.c
)

if (CONFIG_BCACHE)
target_sources(basework
    PRIVATE
    os_bcachedev.c
)
endif()

if (CONFIG_XIPFS)
    target_sources(basework
        PRIVATE
        os_xipfs_impl.cc
    )
endif()

if (CONFIG_PTFS)
    target_sources(basework
        PRIVATE
        # os_ptfs_impl.cc
        os_ptfs.cc
    )
endif()
