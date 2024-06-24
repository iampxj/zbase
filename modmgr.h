/*
 * Copryight 2023 wtcat
 */
#ifndef BASEWORK_UNINSTALL_H_
#define BASEWORK_UNINSTALL_H_

#include <stdbool.h>
#include "basework/container/observer.h"

#ifdef __cplusplus
extern "C"{
#endif

struct module_node {
	struct observer_base base;
	const char *name;
};

/*
 * Module Actions
 */
#define MODULE_INSTALL   1
#define MODULE_UNINSTALL 2 


#define INIT_MODNODE(_m, _name, _fn, _prio) \
do {\
	(_m)->name = _name; \
	(_m)->base = OBSERVER_STATIC_INIT(_fn, _prio); \
} while (0)

/*
 * module_manager_register - Register a module
 *
 * @m: pointer to module-node
 * return 0 if success
 */
int module_manager_register(struct module_node *m);

/*
 * module_manager_control - Send command to all modules that has been reigstered
 *
 * @action: command
 * @filter: filter function
 * @p: user data
 * return 0 if success
 */
int module_manager_control(unsigned long action, 
    bool (*filter)(struct module_node *, void *), 
    void *p);

/*
 * module_manager_init - Initialize module manager
 * return 0 if success
 */
int module_manager_init(void);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_UNINSTALL_H_ */