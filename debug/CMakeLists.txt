if (CONFIG_KASAN)
zephyr_library_named(xdebug)

zephyr_library_compile_options(
    -fno-sanitize=all
)

zephyr_library_sources(
    ${CMAKE_CURRENT_SOURCE_DIR}/kasan.c
)
endif(CONFIG_KASAN)

zephyr_library_sources_ifdef(CONFIG_CORTEXM_DWT 
    ${CMAKE_CURRENT_SOURCE_DIR}/cortexm/dwt.c
)

