PT_ENTRY("bindinfo", BYTE(64)), /* User bind information */
PT_ENTRY("burn_in_main", BYTE(32)), /* User burn_in information */
PT_ENTRY("burn_in_restart", BYTE(32)), /* User burn_in information */
PT_ENTRY("burn_in_cp", BYTE(64)), /* User burn_in information */
PT_ENTRY("burn_in_pd", BYTE(100)), /* User burn_in information */
PT_ENTRY("gnss", KB(6)), /* AGNSS data */
PT_ENTRY("alarm", BYTE(1600)), /* alarm information */
PT_ENTRY("device_info", BYTE(128)), /* device_info information */
PT_ENTRY("perference", BYTE(64)), /* perference information */
PT_ENTRY("language", BYTE(32)), /* language information */
PT_ENTRY("disturb", BYTE(128)), /* disturb information */
PT_ENTRY("screen_brightness", BYTE(32)), /* screen_brightness information */
PT_ENTRY("standing_remind", BYTE(32)), /* standing_remind information */
PT_ENTRY("weather", KB(2)), /* weather information */
PT_ENTRY("world_time", BYTE(400)), /* world_time information */
PT_ENTRY("find_phone", BYTE(8)), /* world_time information */
PT_ENTRY("multi_sport_list", BYTE(256)), /* multi_sport_list information */
PT_ENTRY("multi_sport_history_type", KB(2)), /* multi_sport_history_type information */
PT_ENTRY("app_heart_rate_monitor", BYTE(256)), /* app_heart_rate_monitor information */
PT_ENTRY("app_spo2_monitor", BYTE(256)), /* app_spo2_monitor information */
PT_ENTRY("app_stress_monitor", BYTE(256)), /* app_stress_monitor information */
PT_ENTRY("app_noise_monitor", BYTE(256)), /* app_noise_monitor information */
PT_ENTRY("app_intelligent_recognition", BYTE(32)), /* app_intelligent_recognition information */
PT_ENTRY("app_sleep_monitor", BYTE(32)), /* app_sleep_monitor information */
PT_ENTRY("app_call_switch", BYTE(16)), /* app_sleep_monitor information */
PT_ENTRY("distance_daily_data", BYTE(128)), /* distance_daily_data information */
PT_ENTRY("step_daily_data", BYTE(128)), /* step_daily_data information */
PT_ENTRY("kcal_daily_data", BYTE(256)), /* kcal_daily_data information */
PT_ENTRY("exercised_duration_daily_data", BYTE(128)), /* exercised_duration_daily_data information */
PT_ENTRY("stand_daily_data", BYTE(128)), /* stand_daily_data information */
PT_ENTRY("sleep_record", BYTE(128)), /* stand_daily_data information */
PT_ENTRY("rawdata_monitor", BYTE(64)), /* stand_daily_data information */
PT_ENTRY("app_contacts", KB(2)), /* app_contacts information */
PT_ENTRY("app_global_var", BYTE(32)), /* app_global_var information */
PT_ENTRY("app_tools_card", BYTE(32)), /* app_tools_card information */
PT_ENTRY("app_voice_assistant", BYTE(32)), /* app_voice_assistant information */
PT_ENTRY("activity_weekly", BYTE(256)), /* activity_weekly information */
PT_ENTRY("wtm2101_ota", BYTE(16)), /* wtm2101_ota information */
PT_ENTRY("sport_weekly", BYTE(256)), /* sport_weekly information */
PT_ENTRY("sleep_resistance_restart", KB(4)), /* sleep_resistance_restart information*/
PT_ENTRY("app_heart_rate_zone", BYTE(16)), /* app_heart_rate_zone information*/
PT_ENTRY("app_countdown_record", BYTE(32)), /* app_countdown_record information*/
PT_ENTRY("user_setting_notifications", BYTE(256)), /* user_setting_notifications information*/
PT_ENTRY("motor_vibration", BYTE(256)), /* motor_vibration information*/
PT_ENTRY("sleep_health_restart", BYTE(256)), /* sleep_health_restart information*/
PT_ENTRY("heart_rate_daily_data", BYTE(256)), /* sleep_health_restart information*/
PT_ENTRY("stress_daily_data", BYTE(256)), /* sleep_health_restart information*/
PT_ENTRY("spo2_daily_data", BYTE(256)), /* sleep_health_restart information*/
PT_ENTRY("event_report", KB(3)), /* event_report information*/
PT_ENTRY("screen_brightness_management", BYTE(256)), /* screen_brightness_management information*/
PT_ENTRY("app_ringtone", BYTE(32)), /* app_ringtone information*/
PT_ENTRY("app_menstrual_cycle", KB(3)), /* app_menstrual_cycle information*/
PT_ENTRY("breath_data", BYTE(580)), /* 呼吸训练 information*/
PT_ENTRY("app_emergency_contacts", BYTE(256)), /* app_emergency_contacts information */
PT_ENTRY("app_button_crown", BYTE(32)), /* app_button_crown information */
PT_ENTRY("hrv_daily_data", BYTE(256)), /* sleep_health_restart information*/
PT_ENTRY("mag_cali", BYTE(64)), /* mag information*/
PT_ENTRY("app_quick_card", BYTE(32)), /* app_quick_card information*/
PT_ENTRY("climb_stairs_data", BYTE(256)), /* climb_stairs_data information */
PT_ENTRY("alexa_alarm", BYTE(128)), /* alexa alarm information */
PT_ENTRY("alexa_reminder", BYTE(512)), /* alexa_reminder information */
PT_ENTRY("alexa_timer", BYTE(64)), /* alexa_timer information */
PT_ENTRY("alexa_skill", BYTE(256)), /* alexa_skill information */
PT_ENTRY("app_drink_water_switch", BYTE(64)), /* app_drink_water_switch information */