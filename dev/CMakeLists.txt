
zephyr_library_sources(
    ${CMAKE_CURRENT_SOURCE_DIR}/disk.c
)

if (NOT CONFIG_BOOTLOADER)
zephyr_library_sources(
    ${CMAKE_CURRENT_SOURCE_DIR}/partition_file.c
    ${CMAKE_CURRENT_SOURCE_DIR}/partition.c
    ${CMAKE_CURRENT_SOURCE_DIR}/partition_cfg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/partition_usr_cfg.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gpt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/buffer_io.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rte_timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/fifofs.c
)

if (CONFIG_BLKDEV_LITE)
    zephyr_library_sources(${CMAKE_CURRENT_SOURCE_DIR}/blkdev_lite.c)
else ()
    zephyr_library_sources(${CMAKE_CURRENT_SOURCE_DIR}/blkdev.c)
endif(CONFIG_BLKDEV_LITE)
endif()

if (NOT CONFIG_BOOTLOADER)
zephyr_library_sources_ifdef(CONFIG_PTFS   
    ${CMAKE_CURRENT_SOURCE_DIR}/ptfs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ptfs_ext.c
)
zephyr_library_sources_ifdef(CONFIG_XIPFS  ${CMAKE_CURRENT_SOURCE_DIR}/xipfs.c)
endif()


if (CONFIG_BCACHE)
zephyr_library_sources(
    ${CMAKE_CURRENT_SOURCE_DIR}/bcache.c
)
endif(CONFIG_BCACHE)