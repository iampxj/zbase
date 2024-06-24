/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <errno.h>
#include <stddef.h>

#include "basework/compiler.h"
#include "basework/container/observer.h"

int __rte_notrace observer_register(struct observer_base **nl,
		struct observer_base *n)
{
	while ((*nl) != NULL) {
		if (n->priority > (*nl)->priority)
			break;
		nl = &((*nl)->next);
	}
	n->next = *nl;
	*nl = n;
	return 0;
}

int __rte_notrace observer_cond_register(struct observer_base **nl,
		struct observer_base *n)
{
	while ((*nl) != NULL) {
		if ((*nl) == n) 
			return 0;

		if (n->priority > (*nl)->priority)
			break;
		nl = &((*nl)->next);
	}
	n->next = *nl;
	*nl = n;
	return 0;
}

int __rte_notrace observer_unregister(struct observer_base **nl,
		struct observer_base *n)
{
	while ((*nl) != NULL) {
		if ((*nl) == n) {
			*nl = n->next;
			return 0;
		}
		nl = &((*nl)->next);
	}
	return -EEXIST;
}

int __rte_notrace observer_notify(struct observer_base **nl,
			       unsigned long val, 
			       void *v)
{
	int ret = NOTIFY_DONE;
	struct observer_base *nb, *next_nb;

	nb = *nl;
	while (nb) {
		next_nb = nb->next;
		ret = nb->update(nb, val, v);
		if ((ret & NOTIFY_STOP_MASK) == NOTIFY_STOP_MASK)
			break;
		nb = next_nb;
	}
	return ret;
}
