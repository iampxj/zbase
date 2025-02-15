
if (CONFIG_FILE_SYSTEM_FILEXFS)
zephyr_library_named(filexfs)

if(NOT DEFINED FILEX_PLATFORM_ENV)
    set(FILEX_PLATFORM_ENV generic)
endif()

zephyr_include_directories(
    ${CMAKE_CURRENT_LIST_DIR}/common/inc
    ${CMAKE_CURRENT_LIST_DIR}/ports/${FILEX_PLATFORM_ENV}/inc
)

zephyr_library_compile_options(
    -DFX_INCLUDE_USER_DEFINE_FILE
    # -DFX_STANDALONE_ENABLE
)

if (ZEPHYR_BINARY_DIR)
    zephyr_library_sources(filex_fs.c)
endif()

# Then the common files
add_subdirectory(common)
endif(CONFIG_FILE_SYSTEM_FILEXFS)
