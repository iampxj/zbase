
/*
 * Copyright 2022 creekwearable
 */

#ifndef BASEWORK_ERRNO_H_
#define BASEWORK_ERRNO_H_

#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void __lib_assert_func(const char *file, int line, const char *func, 
    int err_code);

#if defined(__linux__) || defined(_WIN32)
#include <assert.h>
#define APP_ERROR_CHECK(err) assert((err) == 0)
#else
#define APP_ERROR_CHECK(_err) \
    ((_err) != 0)? \
        __lib_assert_func(__FILE__, __LINE__, __func__, _err): 0
#endif /* __linux__ || _WIN32 */

#define ERR_BASE      (0)       ///< Global error base 

#define ERR_SUCCESS                     (0)  ///< Successful command
#define ERR_SVC_HANDLER_MISSING         (ERR_BASE + 1)  ///< SVC handler is missing
#define ERR_SOFTDEVICE_NOT_ENABLED      (ERR_BASE + 2)  ///< SoftDevice has not been enabled
#define ERR_INTERNAL                    (ERR_BASE + 3)  ///< Internal Error
#define ERR_NO_MEM                      (ERR_BASE + 4)  ///< No Memory for operation
#define ERR_NOT_FOUND                   (ERR_BASE + 5)  ///< Not found
#define ERR_NOT_SUPPORTED               (ERR_BASE + 6)  ///< Not supported
#define ERR_INVALID_PARAM               (ERR_BASE + 7)  ///< Invalid Parameter
#define ERR_INVALID_STATE               (ERR_BASE + 8)  ///< Invalid state, operation disallowed in this state
#define ERR_INVALID_LENGTH              (ERR_BASE + 9)  ///< Invalid Length
#define ERR_INVALID_FLAGS               (ERR_BASE + 10) ///< Invalid Flags
#define ERR_INVALID_DATA                (ERR_BASE + 11) ///< Invalid Data
#define ERR_DATA_SIZE                   (ERR_BASE + 12) ///< Invalid Data size
#define ERR_TIMEOUT                     (ERR_BASE + 13) ///< Operation timed out
#define ERR_NULL                        (ERR_BASE + 14) ///< Null Pointer
#define ERR_FORBIDDEN                   (ERR_BASE + 15) ///< Forbidden Operation
#define ERR_INVALID_ADDR                (ERR_BASE + 16) ///< Bad Memory Address
#define ERR_BUSY                        (ERR_BASE + 17) ///< Busy
#define ERR_CONN_COUNT                  (ERR_BASE + 18) ///< Maximum connection count exceeded.
#define ERR_RESOURCES                   (ERR_BASE + 19) ///< Not enough resources for operation
#define ERR_BT_OTA                      (ERR_BASE + 20) ///< Not enough resources for operation
#define ERR_NO_SPACE                    (ERR_BASE + 21) ///< Not enough space for operation


#ifdef __cplusplus
}
#endif
#endif // BASEWORK_ERRNO_H_
