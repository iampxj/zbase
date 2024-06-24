zephyr_library_named(basework_misc)

zephyr_library_sources(empty.c)

if (CONFIG_MISC_WORKOUT)
zephyr_library_sources(
    workout_core.c
    workout_parser.c
    workout_file.c
    workout_spdata.c
    workout_dataimpl.c
    workout_sportaddedlist.c
    workout_sport_attr.c
	# workout_ui_func.c
)
endif()
