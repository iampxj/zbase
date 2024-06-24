/* SPDX-License-Identifier: GPL-2.0-only */

#ifdef  BASEWORK_COMPILER_VERSION_H_
#error "Please do not include <basework/compiler-version.h>. This is done by the build system."
#endif
#define BASEWORK_COMPILER_VERSION_H_

/*
 * This header exists to force full rebuild when the compiler is upgraded.
 *
 * When fixdep scans this, it will find this string "CONFIG_CC_VERSION_TEXT"
 * and add dependency on include/config/CC_VERSION_TEXT, which is touched
 * by Kconfig when the version string from the compiler changes.
 */
