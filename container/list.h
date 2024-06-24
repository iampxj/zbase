/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 wtcat
 */
#ifndef BASE_LIST_H_
#define BASE_LIST_H_

#include <stddef.h>
#include <stdint.h>

#include "basework/generic.h"

#ifdef __cplusplus
extern "C"{
#endif

struct rte_list {
	struct rte_list *next, *prev;
};

struct rte_hlist {
	struct rte_hnode *first;
};

struct rte_hnode {
	struct rte_hnode *next, **pprev;
};

#define RTE_LIST_POISON1 NULL
#define RTE_LIST_POISON2 NULL

#ifndef RTE_WRITE_ONCE
#define RTE_WRITE_ONCE(p, v) (p) = (v)
#endif

#ifndef RTE_READ_ONCE
#define RTE_READ_ONCE(p) (p)
#endif

/*
 * Simple doubly linked list implementation.
 *
 * Some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */

#define RTE_LIST_INIT(name) { &(name), &(name) }

#define RTE_LIST(name) \
	struct rte_list name = RTE_LIST_INIT(name)

static inline void RTE_INIT_LIST(struct rte_list *list)
{
	RTE_WRITE_ONCE(list->next, list);
	list->prev = list;
}

/*
 * Insert a newn entry between two known consecutive entries.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __rte_list_add(struct rte_list *newn,
			      struct rte_list *prev,
			      struct rte_list *next)
{
	next->prev = newn;
	newn->next = next;
	newn->prev = prev;
	RTE_WRITE_ONCE(prev->next, newn);
}

/**
 * rte_list_add - add a newn entry
 * @newn: newn entry to be added
 * @head: list head to add it after
 *
 * Insert a newn entry after the specified head.
 * This is good for implementing stacks.
 */
static inline void rte_list_add(struct rte_list *newn, struct rte_list *head)
{
	__rte_list_add(newn, head, head->next);
}


/**
 * rte_list_add_tail - add a newn entry
 * @newn: newn entry to be added
 * @head: list head to add it before
 *
 * Insert a newn entry before the specified head.
 * This is useful for implementing queues.
 */
static inline void rte_list_add_tail(struct rte_list *newn, struct rte_list *head)
{
	__rte_list_add(newn, head->prev, head);
}

/*
 * Delete a list entry by making the prev/next entries
 * point to each other.
 *
 * This is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __rte_list_del(struct rte_list * prev, struct rte_list * next)
{
	next->prev = prev;
	RTE_WRITE_ONCE(prev->next, next);
}

/**
 * rte_list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: rte_list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void __rte_list_del_entry(struct rte_list *entry)
{
	__rte_list_del(entry->prev, entry->next);
}

static inline void rte_list_del(struct rte_list *entry)
{
	__rte_list_del_entry(entry);
	entry->next = RTE_LIST_POISON1;
	entry->prev = RTE_LIST_POISON2;
}

/**
 * rte_list_replace - replace old entry by newn one
 * @old : the element to be replaced
 * @newn : the newn element to insert
 *
 * If @old was empty, it will be overwritten.
 */
static inline void rte_list_replace(struct rte_list *old,
				struct rte_list *newn)
{
	newn->next = old->next;
	newn->next->prev = newn;
	newn->prev = old->prev;
	newn->prev->next = newn;
}

static inline void rte_list_replace_init(struct rte_list *old,
					struct rte_list *newn)
{
	rte_list_replace(old, newn);
	RTE_INIT_LIST(old);
}

/**
 * rte_list_del_init - deletes entry from list and reinitialize it.
 * @entry: the element to delete from the list.
 */
static inline void rte_list_del_init(struct rte_list *entry)
{
	__rte_list_del_entry(entry);
	RTE_INIT_LIST(entry);
}

/**
 * rte_list_move - delete from one list and add as another's head
 * @list: the entry to move
 * @head: the head that will precede our entry
 */
static inline void rte_list_move(struct rte_list *list, struct rte_list *head)
{
	__rte_list_del_entry(list);
	rte_list_add(list, head);
}

/**
 * rte_list_move_tail - delete from one list and add as another's tail
 * @list: the entry to move
 * @head: the head that will follow our entry
 */
static inline void rte_list_move_tail(struct rte_list *list,
				  struct rte_list *head)
{
	__rte_list_del_entry(list);
	rte_list_add_tail(list, head);
}

/**
 * rte_list_is_last - tests whether @list is the last entry in list @head
 * @list: the entry to test
 * @head: the head of the list
 */
static inline int rte_list_is_last(const struct rte_list *list,
				const struct rte_list *head)
{
	return list->next == head;
}

/**
 * rte_list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int rte_list_empty(const struct rte_list *head)
{
	return RTE_READ_ONCE(head->next) == head;
}

/**
 * rte_list_empty_careful - tests whether a list is empty and not being modified
 * @head: the list to test
 *
 * Description:
 * tests whether a list is empty _and_ checks that no other CPU might be
 * in the process of modifying either member (next or prev)
 *
 * NOTE: using rte_list_empty_careful() without synchronization
 * can only be safe if the only activity that can happen
 * to the list entry is rte_list_del_init(). Eg. it cannot be used
 * if another CPU could re-rte_list_add() it.
 */
static inline int rte_list_empty_careful(const struct rte_list *head)
{
	struct rte_list *next = head->next;
	return (next == head) && (next == head->prev);
}

/**
 * rte_list_rotate_left - rotate the list to the left
 * @head: the head of the list
 */
static inline void rte_list_rotate_left(struct rte_list *head)
{
	struct rte_list *first;

	if (!rte_list_empty(head)) {
		first = head->next;
		rte_list_move_tail(first, head);
	}
}

/**
 * rte_list_is_singular - tests whether a list has just one entry.
 * @head: the list to test.
 */
static inline int rte_list_is_singular(const struct rte_list *head)
{
	return !rte_list_empty(head) && (head->next == head->prev);
}

static inline void __rte_list_cut_position(struct rte_list *list,
		struct rte_list *head, struct rte_list *entry)
{
	struct rte_list *new_first = entry->next;
	list->next = head->next;
	list->next->prev = list;
	list->prev = entry;
	entry->next = list;
	head->next = new_first;
	new_first->prev = head;
}

/**
 * rte_list_cut_position - cut a list into two
 * @list: a newn list to add all removed entries
 * @head: a list with entries
 * @entry: an entry within head, could be the head itself
 *	and if so we won't cut the list
 *
 * This helper moves the initial part of @head, up to and
 * including @entry, from @head to @list. You should
 * pass on @entry an element you know is on @head. @list
 * should be an empty list or a list you do not care about
 * losing its data.
 *
 */
static inline void rte_list_cut_position(struct rte_list *list,
		struct rte_list *head, struct rte_list *entry)
{
	if (rte_list_empty(head))
		return;
	if (rte_list_is_singular(head) &&
		(head->next != entry && head != entry))
		return;
	if (entry == head)
		RTE_INIT_LIST(list);
	else
		__rte_list_cut_position(list, head, entry);
}

/**
 * rte_list_cut_before - cut a list into two, before given entry
 * @list: a newn list to add all removed entries
 * @head: a list with entries
 * @entry: an entry within head, could be the head itself
 *
 * This helper moves the initial part of @head, up to but
 * excluding @entry, from @head to @list.  You should pass
 * in @entry an element you know is on @head.  @list should
 * be an empty list or a list you do not care about losing
 * its data.
 * If @entry == @head, all entries on @head are moved to
 * @list.
 */
static inline void rte_list_cut_before(struct rte_list *list,
				   struct rte_list *head,
				   struct rte_list *entry)
{
	if (head->next == entry) {
		RTE_INIT_LIST(list);
		return;
	}
	list->next = head->next;
	list->next->prev = list;
	list->prev = entry->prev;
	list->prev->next = list;
	head->next = entry;
	entry->prev = head;
}

static inline void __rte_list_splice(const struct rte_list *list,
				 struct rte_list *prev,
				 struct rte_list *next)
{
	struct rte_list *first = list->next;
	struct rte_list *last = list->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

/**
 * rte_list_splice - join two lists, this is designed for stacks
 * @list: the newn list to add.
 * @head: the place to add it in the first list.
 */
static inline void rte_list_splice(const struct rte_list *list,
				struct rte_list *head)
{
	if (!rte_list_empty(list))
		__rte_list_splice(list, head, head->next);
}

/**
 * rte_list_splice_tail - join two lists, each list being a queue
 * @list: the newn list to add.
 * @head: the place to add it in the first list.
 */
static inline void rte_list_splice_tail(struct rte_list *list,
				struct rte_list *head)
{
	if (!rte_list_empty(list))
		__rte_list_splice(list, head->prev, head);
}

/**
 * rte_list_splice_init - join two lists and reinitialise the emptied list.
 * @list: the newn list to add.
 * @head: the place to add it in the first list.
 *
 * The list at @list is reinitialised
 */
static inline void rte_list_splice_init(struct rte_list *list,
				    struct rte_list *head)
{
	if (!rte_list_empty(list)) {
		__rte_list_splice(list, head, head->next);
		RTE_INIT_LIST(list);
	}
}

/**
 * rte_list_splice_tail_init - join two lists and reinitialise the emptied list
 * @list: the newn list to add.
 * @head: the place to add it in the first list.
 *
 * Each of the lists is a queue.
 * The list at @list is reinitialised
 */
static inline void rte_list_splice_tail_init(struct rte_list *list,
					 struct rte_list *head)
{
	if (!rte_list_empty(list)) {
		__rte_list_splice(list, head->prev, head);
		RTE_INIT_LIST(list);
	}
}

/**
 * rte_list_entry - get the struct for this entry
 * @ptr:	the &struct rte_list pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the rte_list within the struct.
 */
#define rte_list_entry(ptr, type, member) \
	container_of(ptr, type, member)

/**
 * rte_list_first_entry - get the first element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the rte_list within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define rte_list_first_entry(ptr, type, member) \
	rte_list_entry((ptr)->next, type, member)

/**
 * rte_list_last_entry - get the last element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the rte_list within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define rte_list_last_entry(ptr, type, member) \
	rte_list_entry((ptr)->prev, type, member)

/**
 * rte_list_first_entry_or_null - get the first element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the rte_list within the struct.
 *
 * Note that if the list is empty, it returns NULL.
 */
#define rte_list_first_entry_or_null(ptr, type, member) ({ \
	struct rte_list *head__ = (ptr); \
	struct rte_list *pos__ = RTE_READ_ONCE(head__->next); \
	pos__ != head__ ? rte_list_entry(pos__, type, member) : NULL; \
})

/**
 * rte_list_next_entry - get the next element in list
 * @pos:	the type * to cursor
 * @member:	the name of the rte_list within the struct.
 */
#define rte_list_next_entry(pos, member) \
	rte_list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * rte_list_prev_entry - get the prev element in list
 * @pos:	the type * to cursor
 * @member:	the name of the rte_list within the struct.
 */
#define rte_list_prev_entry(pos, member) \
	rte_list_entry((pos)->member.prev, typeof(*(pos)), member)

/**
 * rte_list_foreach	-	iterate over a list
 * @pos:	the &struct rte_list to use as a loop cursor.
 * @head:	the head for your list.
 */
#define rte_list_foreach(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * rte_list_foreach_prev	-	iterate over a list backwards
 * @pos:	the &struct rte_list to use as a loop cursor.
 * @head:	the head for your list.
 */
#define rte_list_foreach_prev(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

/**
 * rte_list_foreach_safe - iterate over a list safe against removal of list entry
 * @pos:	the &struct rte_list to use as a loop cursor.
 * @n:		another &struct rte_list to use as temporary storage
 * @head:	the head for your list.
 */
#define rte_list_foreach_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

/**
 * rte_list_foreach_prev_safe - iterate over a list backwards safe against removal of list entry
 * @pos:	the &struct rte_list to use as a loop cursor.
 * @n:		another &struct rte_list to use as temporary storage
 * @head:	the head for your list.
 */
#define rte_list_foreach_prev_safe(pos, n, head) \
	for (pos = (head)->prev, n = pos->prev; \
	     pos != (head); \
	     pos = n, n = pos->prev)

/**
 * rte_list_foreach_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the rte_list within the struct.
 */
#define rte_list_foreach_entry(pos, head, member)				\
	for (pos = rte_list_first_entry(head, typeof(*pos), member);	\
	     &pos->member != (head);					\
	     pos = rte_list_next_entry(pos, member))

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the rte_list within the struct.
 */
#define list_for_each_entry_reverse(pos, head, member)			\
	for (pos = rte_list_last_entry(head, typeof(*pos), member);		\
	     &pos->member != (head); 					\
	     pos = rte_list_prev_entry(pos, member))

/**
 * rte_list_prepare_entry - prepare a pos entry for use in rte_list_foreach_entry_continue()
 * @pos:	the type * to use as a start point
 * @head:	the head of the list
 * @member:	the name of the rte_list within the struct.
 *
 * Prepares a pos entry for use as a start point in rte_list_foreach_entry_continue().
 */
#define rte_list_prepare_entry(pos, head, member) \
	((pos) ? : rte_list_entry(head, typeof(*pos), member))

/**
 * rte_list_foreach_entry_continue - continue iteration over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the rte_list within the struct.
 *
 * Continue to iterate over list of given type, continuing after
 * the current position.
 */
#define rte_list_foreach_entry_continue(pos, head, member) 		\
	for (pos = rte_list_next_entry(pos, member);			\
	     &pos->member != (head);					\
	     pos = rte_list_next_entry(pos, member))

/**
 * rte_list_foreach_entry_continue_reverse - iterate backwards from the given point
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the rte_list within the struct.
 *
 * Start to iterate over list of given type backwards, continuing after
 * the current position.
 */
#define rte_list_foreach_entry_continue_reverse(pos, head, member)		\
	for (pos = rte_list_prev_entry(pos, member);			\
	     &pos->member != (head);					\
	     pos = rte_list_prev_entry(pos, member))

/**
 * rte_list_foreach_entry_from - iterate over list of given type from the current point
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the rte_list within the struct.
 *
 * Iterate over list of given type, continuing from current position.
 */
#define rte_list_foreach_entry_from(pos, head, member) 			\
	for (; &pos->member != (head);					\
	     pos = rte_list_next_entry(pos, member))

/**
 * rte_list_foreach_entry_from_reverse - iterate backwards over list of given type
 *                                    from the current point
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the rte_list within the struct.
 *
 * Iterate backwards over list of given type, continuing from current position.
 */
#define rte_list_foreach_entry_from_reverse(pos, head, member)		\
	for (; &pos->member != (head);					\
	     pos = rte_list_prev_entry(pos, member))

/**
 * rte_list_foreach_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the rte_list within the struct.
 */
#define rte_list_foreach_entry_safe(pos, n, head, member)			\
	for (pos = rte_list_first_entry(head, typeof(*pos), member),	\
		n = rte_list_next_entry(pos, member);			\
	     &pos->member != (head); 					\
	     pos = n, n = rte_list_next_entry(n, member))

/**
 * rte_list_foreach_entry_safe_continue - continue list iteration safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the rte_list within the struct.
 *
 * Iterate over list of given type, continuing after current point,
 * safe against removal of list entry.
 */
#define rte_list_foreach_entry_safe_continue(pos, n, head, member) 		\
	for (pos = rte_list_next_entry(pos, member), 				\
		n = rte_list_next_entry(pos, member);				\
	     &pos->member != (head);						\
	     pos = n, n = rte_list_next_entry(n, member))

/**
 * rte_list_foreach_entry_safe_from - iterate over list from current point safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the rte_list within the struct.
 *
 * Iterate over list of given type from current point, safe against
 * removal of list entry.
 */
#define rte_list_foreach_entry_safe_from(pos, n, head, member) 			\
	for (n = rte_list_next_entry(pos, member);					\
	     &pos->member != (head);						\
	     pos = n, n = rte_list_next_entry(n, member))

/**
 * rte_list_foreach_entry_safe_reverse - iterate backwards over list safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the rte_list within the struct.
 *
 * Iterate backwards over list of given type, safe against removal
 * of list entry.
 */
#define rte_list_foreach_entry_safe_reverse(pos, n, head, member)		\
	for (pos = rte_list_last_entry(head, typeof(*pos), member),		\
		n = rte_list_prev_entry(pos, member);			\
	     &pos->member != (head); 					\
	     pos = n, n = rte_list_prev_entry(n, member))

/**
 * rte_list_safe_reset_next - reset a stale rte_list_foreach_entry_safe loop
 * @pos:	the loop cursor used in the rte_list_foreach_entry_safe loop
 * @n:		temporary storage used in rte_list_foreach_entry_safe
 * @member:	the name of the rte_list within the struct.
 *
 * rte_list_safe_reset_next is not safe to use in general if the list may be
 * modified concurrently (eg. the lock is dropped in the loop body). An
 * exception to this is if the cursor element (pos) is pinned in the list,
 * and rte_list_safe_reset_next is called after re-taking the lock and before
 * completing the current iteration of the loop body.
 */
#define rte_list_safe_reset_next(pos, n, member)				\
	n = rte_list_next_entry(pos, member)

/*
 * Double linked lists with a single pointer list head.
 * Mostly useful for hash tables where the two pointer list head is
 * too wasteful.
 * You lose the ability to access the tail in O(1).
 */

#define RTE_HLIST_INIT { .first = NULL }
#define RTE_HLIST(name) struct rte_hlist name = {  .first = NULL }
#define RTE_INIT_HLIST(ptr) ((ptr)->first = NULL)
static inline void RTE_INIT_HLIST_NODE(struct rte_hnode *h)
{
	h->next = NULL;
	h->pprev = NULL;
}

static inline int rte_hlist_unhashed(const struct rte_hnode *h)
{
	return !h->pprev;
}

static inline int rte_hlist_empty(const struct rte_hlist *h)
{
	return !RTE_READ_ONCE(h->first);
}

static inline void __rte_hlist_del(struct rte_hnode *n)
{
	struct rte_hnode *next = n->next;
	struct rte_hnode **pprev = n->pprev;

	RTE_WRITE_ONCE(*pprev, next);
	if (next)
		next->pprev = pprev;
}

static inline void rte_hlist_del(struct rte_hnode *n)
{
	__rte_hlist_del(n);
	n->next = RTE_LIST_POISON1;
	n->pprev = RTE_LIST_POISON2;
}

static inline void rte_hlist_del_init(struct rte_hnode *n)
{
	if (!rte_hlist_unhashed(n)) {
		__rte_hlist_del(n);
		RTE_INIT_HLIST_NODE(n);
	}
}

static inline void rte_hlist_add_head(struct rte_hnode *n, struct rte_hlist *h)
{
	struct rte_hnode *first = h->first;
	n->next = first;
	if (first)
		first->pprev = &n->next;
	RTE_WRITE_ONCE(h->first, n);
	n->pprev = &h->first;
}

/* next must be != NULL */
static inline void rte_hlist_add_before(struct rte_hnode *n,
					struct rte_hnode *next)
{
	n->pprev = next->pprev;
	n->next = next;
	next->pprev = &n->next;
	RTE_WRITE_ONCE(*(n->pprev), n);
}

static inline void rte_hlist_add_behind(struct rte_hnode *n,
				    struct rte_hnode *prev)
{
	n->next = prev->next;
	RTE_WRITE_ONCE(prev->next, n);
	n->pprev = &prev->next;

	if (n->next)
		n->next->pprev  = &n->next;
}

/* after that we'll appear to be on some hlist and rte_hlist_del will work */
static inline void rte_hlist_add_fake(struct rte_hnode *n)
{
	n->pprev = &n->next;
}

static inline bool rte_hlist_fake(struct rte_hnode *h)
{
	return h->pprev == &h->next;
}

/*
 * Check whether the node is the only node of the head without
 * accessing head:
 */
static inline bool
rte_hlist_is_singular_node(struct rte_hnode *n, struct rte_hlist *h)
{
	return !n->next && n->pprev == &h->first;
}

/*
 * Move a list from one list head to another. Fixup the pprev
 * reference of the first entry if it exists.
 */
static inline void rte_hlist_move_list(struct rte_hlist *old,
				   struct rte_hlist *newn)
{
	newn->first = old->first;
	if (newn->first)
		newn->first->pprev = &newn->first;
	old->first = NULL;
}

#define rte_hlist_entry(ptr, type, member) rte_container_of(ptr, type, member)

#define rte_hlist_foreach(pos, head) \
	for (pos = (head)->first; pos ; pos = pos->next)

#define rte_hlist_foreach_safe(pos, n, head) \
	for (pos = (head)->first; pos && ({ n = pos->next; 1; }); \
	     pos = n)

#define rte_hlist_entry_safe(ptr, type, member) \
	({ typeof(ptr) ____ptr = (ptr); \
	   ____ptr ? rte_hlist_entry(____ptr, type, member) : NULL; \
	})

/**
 * rte_hlist_foreach_entry	- iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the rte_hnode within the struct.
 */
#define rte_hlist_foreach_entry(pos, head, member)				\
	for (pos = rte_hlist_entry_safe((head)->first, typeof(*(pos)), member);\
	     pos;							\
	     pos = rte_hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

/**
 * rte_hlist_foreach_entry_continue - iterate over a hlist continuing after current point
 * @pos:	the type * to use as a loop cursor.
 * @member:	the name of the rte_hnode within the struct.
 */
#define rte_hlist_foreach_entry_continue(pos, member)			\
	for (pos = rte_hlist_entry_safe((pos)->member.next, typeof(*(pos)), member);\
	     pos;							\
	     pos = rte_hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

/**
 * rte_hlist_foreach_entry_from - iterate over a hlist continuing from current point
 * @pos:	the type * to use as a loop cursor.
 * @member:	the name of the rte_hnode within the struct.
 */
#define rte_hlist_foreach_entry_from(pos, member)				\
	for (; pos;							\
	     pos = rte_hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

/**
 * rte_hlist_foreach_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another &struct rte_hnode to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the rte_hnode within the struct.
 */
#define rte_hlist_foreach_entry_safe(pos, n, head, member) 		\
	for (pos = rte_hlist_entry_safe((head)->first, typeof(*pos), member);\
	     pos && ({ n = pos->member.next; 1; });			\
	     pos = rte_hlist_entry_safe(n, typeof(*pos), member))

#ifdef __cplusplus
}
#endif
#endif /* BASE_LIST_H_ */