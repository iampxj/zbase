#ifdef CONFIG_WTM2101AC
    PT_ENTRY("trim",    BYTE(600)),
#endif
PT_ENTRY("sn", 	    BYTE(200)),
PT_ENTRY("sar_calibration", BYTE(64)),
PT_ENTRY("barometer_calibration", BYTE(64)),
PT_ENTRY("burn_in_main_ex", BYTE(32)), /* User burn_in information */
PT_ENTRY("burn_in_restart_ex", BYTE(32)), /* User burn_in information */
PT_ENTRY("burn_in_cp_ex", BYTE(64)), /* User burn_in information */
PT_ENTRY("burn_in_pd_ex", BYTE(100)), /* User burn_in information */
PT_ENTRY("factory_process", BYTE(64)), /* User burn_in information */
