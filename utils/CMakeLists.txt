zephyr_library_sources(
    ${CMAKE_CURRENT_SOURCE_DIR}/ota_fstream.c
)
if (NOT CONFIG_BOOTLOADER)
zephyr_library_sources(
    ${CMAKE_CURRENT_SOURCE_DIR}/ptbin_reader.c
)
endif()