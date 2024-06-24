#include <assert.h>
#include <stdio.h>

#include "basework/ccinit.h"
#include "basework/log.h"

extern "C" int cstd_fs_init(void);

extern "C" void __assert_failed(const char* file, int line, const char* func,
	const char* failedexpr) {
	(void)func;
	(void)failedexpr;
	printf("Assert failed: %s@line\n", file, line);
	assert(0);
}

CC_INIT(win32_platform, kApplicationOrder, 0) {
	static struct printer log_printer;
	printf_format_init(&log_printer);
	pr_log_init(&log_printer);
	cstd_fs_init();
	return 0;
}