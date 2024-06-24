/*
 * Copyright (C) 2001 Momchil Velikov
 * Portions Copyright (C) 2001 Christoph Hellwig
 * Copyright (C) 2006 Nick Piggin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
 /*
  * Copyright 2023 wtcat 
  */
#ifndef BASEWORK_RADIX_TREE_H_
#define BASEWORK_RADIX_TREE_H_

#include "basework/generic.h"

#ifdef __cplusplus
extern "C"{
#endif

struct radix_tree_node;
struct radix_tree_root {
	unsigned int height;
	struct radix_tree_node *rnode;
};

#define INIT_RADIX_TREE(root, mask)	\
do {\
	(root)->height = 0;	\
	(root)->rnode = NULL; \
} while (0)

int radix_tree_insert(struct radix_tree_root *, unsigned long, void *);
void *radix_tree_lookup(struct radix_tree_root *, unsigned long);
void **radix_tree_lookup_slot(struct radix_tree_root *, unsigned long);
void *radix_tree_delete(struct radix_tree_root *, unsigned long);
void radix_tree_init(void);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_RADIX_TREE_H_ */