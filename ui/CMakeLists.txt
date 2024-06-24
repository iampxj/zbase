zephyr_library_named(basework_ui)

zephyr_library_sources(
	msg_view_cache.c
)
if (CONFIG_LVGL_ENCODER)
zephyr_library_sources(
	lvgl_encoder.c
	lvgl_encoder_evt.c
)
endif()

if (CONFIG_LVGL_LAZY_DECOMP)
zephyr_library_sources(
	lvgl_lazydecomp.c
)
endif()

add_subdirectory(widget)
zephyr_library_sources_ifdef(CONFIG_MEM_MONITOR  lvgl_mon.c)
zephyr_library_link_libraries(lvgl)