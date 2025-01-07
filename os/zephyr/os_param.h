/*
 * Copyright 2024 wtcat
 * Global dynamic configure information
 */
#ifndef OS_PARAM_H_
#define OS_PARAM_H_

#include "basework/param.h"

#ifdef __cplusplus
extern "C"{
#endif

#define getApplicationName()  sys_getenv_str("AppLink", "Fitbeing")
#define getBluetoothName()    sys_getenv_str("BtName", "CreekBT")
#define getFirmwareVersion()  sys_getenv_str("FwVersion", "V0.0.0")
#define getDeviceID()         sys_getenv_val("TargetDevID", 1000, 10)

/*
 * @brief  Get logo type
 * @return ("Picture", "FrameAnim", "FadeAnim")
 */
#define getLogoType()         sys_getenv_str("LogoType", "Picture")
#define getLogoPositionX()    sys_getenv_val("LogoX", 0, 10)
#define getLogoPositionY()    sys_getenv_val("LogoY", 0, 10)
#define getLogoAnimTime()     sys_getenv_val("LogoAnimTime", 0, 10)
#define getLogoAnimTimes()    sys_getenv_val("LogoAnimTimes", 0, 10)

/*
 * @brief  Get regulatory information status
 * @return ("okay", "disabled")
 */
#define getRegulatoryStatus() sys_getenv_str("RegulatoryStatus", "okay")
#define getRegulatoryFCC()    sys_getenv_str("RegulatoryFCC", "xxxxxxxxxx")
#define getRegulatoryICC()    sys_getenv_str("RegulatoryICC", "xxxxxxxxxx")

#ifdef __cplusplus
}
#endif
#endif /* OS_PARAM_H_ */
