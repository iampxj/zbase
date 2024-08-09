
zephyr_include_directories(.)

if (${CONFIG_ARCH} STREQUAL "arm")
zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../arch/arm)
endif()

if (NOT CONFIG_BOOTLOADER)
zephyr_library_sources(
    os_timer_impl.c
    os_fs_impl.c
    os_logdisk.c
    os_shellcmd.c
    os_ota.c
    os_rq.c
    os_hack.c
    os_bus.c
)
zephyr_library_sources_ifdef(CONFIG_BCACHE
    os_bcachedev.c
)
zephyr_library_sources_ifdef(CONFIG_GPT_UPDATE
    os_gpt_param.c
)
zephyr_library_sources_ifdef(CONFIG_PTFS           os_ptfs_impl.c os_ptfs.c)
zephyr_library_sources_ifdef(CONFIG_XIPFS          os_xipfs_impl.c)
zephyr_library_sources_ifdef(CONFIG_SCREEN_MANAGER os_screen.c)
zephyr_library_sources_ifdef(CONFIG_MEM_MONITOR    os_monitor.c)

if (CONFIG_FTRACE)
    # target_compile_options(fntrace
    #     INTERFACE
    #     -finstrument-functions
    #     # -finstrument-functions-exclude-file-list=framework/,application/,zephyr/,thirdparty/,middleware/tool/,middleware/factory/,middleware/json,middleware/hal_if/ymodem/,middleware/hal_if/jtag/,basework/ui/,sbc.c,env_core.c,env.c,observer.c,protocol_alarm.pb-c.c,protocol_bind.pb-c.c,protocol_call.pb-c.c,protocol_connect_status.pb-c.c,protocol_device_info.pb-c.c,protocol_device_time.pb-c.c,protocol_disturb.pb-c.c,protocol_drink_water.pb-c.c,protocol_exercise.pb-c.c,protocol_find_phone_watch.pb-c.c,protocol_frequent_contacts.pb-c.c,protocol_health_data_sync.pb-c.c,protocol_health_get_size.pb-c.c,protocol_health_monitor.pb-c.c,protocol_language.pb-c.c,protocol_log_tran.pb-c.c,protocol_menstruation.pb-c.c,protocol_message_notify.pb-c.c,protocol_music_control.pb-c.c,protocol_notice_update.pb-c.c,protocol_quick_card.pb-c.c,protocol_schedule.pb-c.c,protocol_screen_brightness.pb-c.c,protocol_sensor_rawdata_tran.pb-c.c,protocol_set_user_info.pb-c.c,protocol_sleep_monitor.pb-c.c,protocol_standing_remind.pb-c.c,protocol_voice_assistant.pb-c.c,protocol_watch_dial_plate.pb-c.c,protocol_weather.pb-c.c,protocol_world_time.pb-c.c,common.pb-c.c
    #     # -finstrument-functions-exclude-function-list=virt_format,user_fault_init
    # )
    zephyr_library_sources(os_callpath.c)
endif()

if (CONFIG_FAT_FILESYSTEM_ELM)
    zephyr_library_link_libraries(ELMFAT)
endif()
if (CONFIG_FILE_SYSTEM_LITTLEFS)
    zephyr_library_link_libraries(LITTLEFS)
endif()
endif()

zephyr_library_sources(
    os_platform.c
)

