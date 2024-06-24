/*
 * Copyright 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) fmt
#include "basework/log.h"
#include "basework/os/osapi.h"
#include "basework/hook.h"


void RTE_WEAKHOOK(__lib_assert_post, const char *file, int line, int err) {
    (void) file;
    (void) line;
    (void) err;
}

void __lib_assert_func(const char *file, int line, const char *func, 
    int err_code) {
    __lib_assert_post(file, line, err_code);
    pr_emerg("Assertion \"%d\" failed: file \"%s\", line %d%s%s\n",
        err_code,
        file,
        line,
        (func) ? ", function: " : "",
        (func) ? func : ""
    );
    os_panic();

    /* Never reached here */
}

void __assert_func(const char *file, int line, const char *func, const char *expr) {
    __lib_assert_post(file, line, -1);
    pr_emerg("Assertion \"%s\" failed: file \"%s\", line %d %s\n",
        expr, file, line, (func)? func : "");
    os_panic();
    while (1);
}

void RTE_WEAKHOOK(__assert_failed, const char *file, int line, 
    const char *func, const char *failedexpr) {
    pr_emerg("assertion \"%s\" failed: file \"%s\", line %d%s%s\n",
	   failedexpr, file, line,
	   func ? ", function: " : "", func ? func : "");
    os_panic();
}
