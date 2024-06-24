/*
 * Copyright 2022 wtcat
 */
#ifndef BASEWORK_LIB_IOVPR_H_
#define BASEWORK_LIB_IOVPR_H_

#include <stdarg.h>
#include "basework/generic.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef void (*vio_put_char)( int c, void *arg );

int _IO_Vprintf(vio_put_char put_char, void *arg, char const *fmt, va_list ap);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_LIB_IOVPR_H_ */
