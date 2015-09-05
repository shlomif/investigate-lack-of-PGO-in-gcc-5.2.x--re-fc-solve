/* Produced by texiweb from libavl.w. */

/* libavl - library for manipulation of binary trees.
   Copyright (C) 1998, 1999, 2000, 2001, 2002, 2004 Free Software
   Foundation, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301 USA.
*/

#ifndef AVL_H
#define AVL_H 1

#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "meta_alloc.h"

#if SIZEOF_VOID_P == 4
#define WITH_AVL_BALANCE_FIELD 1
#endif

#ifdef FCS_LIBAVL_STORE_WHOLE_KEYS
#include "delta_states.h"
#endif

/* Function types. */
typedef int avl_comparison_func (const void *avl_a, const void *avl_b,
                                 void *avl_param);
typedef void avl_item_func (void *avl_item, void *avl_param);
typedef void *avl_copy_func (void *avl_item, void *avl_param);

#if 0
#ifndef LIBAVL_ALLOCATOR
#define LIBAVL_ALLOCATOR
/* Memory allocator. */
struct libavl_allocator
  {
    void *(*libavl_malloc) (struct libavl_allocator *, size_t libavl_size);
    void (*libavl_free) (struct libavl_allocator *, void *libavl_block);
  };
#endif

/* Default memory allocator. */
extern struct libavl_allocator avl_allocator_default;
void *avl_malloc (struct libavl_allocator *, size_t);
void avl_free (struct libavl_allocator *, void *);
#endif

/* Maximum AVL tree height. */
#ifndef AVL_MAX_HEIGHT
#define AVL_MAX_HEIGHT 92
#endif

#ifdef FCS_LIBAVL_STORE_WHOLE_KEYS
typedef fcs_dbm_record_t avl_key_t;
#define AVL_KEY_PTR_PTR(p) (p)
#define NODE_DATA_PTR(p) (&((p)->avl_data))
#define NODE_ASSIGN_DATA_PTR(node_p, ptr) (((node_p)->avl_data) = *(fcs_dbm_record_t *)ptr)
#define AVL_KEY_ASSIGN_DATA_PTR(p, ptr) ((*(p)) = *(fcs_dbm_record_t *)ptr)
#define AVL_KEY_EQUAL_TO_PTR(key, ptr) (!memcmp(&(key), (ptr), sizeof(key)))
#else
typedef void * avl_key_t;
#define AVL_KEY_PTR_PTR(p) (*(p))
#define NODE_DATA_PTR(p) ((p)->avl_data)
#define NODE_ASSIGN_DATA_PTR(node_p, ptr) (((node_p)->avl_data) = ptr)
#define AVL_KEY_ASSIGN_DATA_PTR(p, ptr) ((*(p)) = ptr)
#define AVL_KEY_EQUAL_TO_PTR(key, ptr) ((key) == (ptr))
#endif

#define AVL_NEXT(p) (*((struct avl_node * *)(NODE_DATA_PTR(p))))
#define AVL_SET_NEXT(p, val) (AVL_NEXT(p) = (val))
/* Tree data structure. */
#define avl_root avl_proto_root.avl_mylink[0]
#define TREE_AVL_ROOT(tree) ((struct avl_node *)((tree)->avl_root))
#define SET_TREE_AVL_ROOT(tree, val) ((tree)->avl_root = (uintptr_t)val)

/* An AVL tree node. */
struct avl_node
  {
    avl_key_t avl_data;                /* Pointer to data. */
    uintptr_t avl_mylink[2];  /* Subtrees. */
#ifdef WITH_AVL_BALANCE_FIELD
    signed char avl_balance;       /* Balance factor. */
#endif
  };

struct avl_table
  {
    struct avl_node avl_proto_root;     /* Tree's root. */
    avl_comparison_func *avl_compare;   /* Comparison function. */
    void *avl_param;                    /* Extra argument to |avl_compare|. */
    fcs_compact_allocator_t avl_allocator;
    struct avl_node * * avl_recycle_bin;
    size_t avl_count;                   /* Number of items in tree. */
    unsigned long avl_generation;       /* Generation number. */
  };

#define L(node, i) (avl_process_link(node->avl_mylink[i]))
#define SET_L(node, i, val) (avl_set_link(node, i, val))
#define NODEPTR_GET_BALANCE(p) avl_get_balance(p)
#define NODEPTR_SET_BALANCE(p, b) avl_set_balance((p), (b))
/* AVL traverser structure. */
struct avl_traverser
  {
    struct avl_table *avl_table;        /* Tree being traversed. */
    struct avl_node *avl_node;          /* Current node in tree. */
    struct avl_node *avl_stack[AVL_MAX_HEIGHT];
                                        /* All the nodes above |avl_node|. */
    size_t avl_height;                  /* Number of nodes in |avl_parent|. */
    unsigned long avl_generation;       /* Generation number. */
  };

/* Table functions. */
struct avl_table *avl_create (avl_comparison_func *, void *, fcs_meta_compact_allocator_t *, void * * common_recycle_bin);
struct avl_table *avl_copy (const struct avl_table *, avl_copy_func *,
                            avl_item_func *);
void avl_destroy (struct avl_table *, avl_item_func *);
avl_key_t * avl_probe (struct avl_table *, void *);
void *avl_insert (struct avl_table *, void *);
void *avl_replace (struct avl_table *, void *);
void *avl_delete (struct avl_table *, const void *);
void *avl_find (const struct avl_table *, const void *);
void avl_assert_insert (struct avl_table *, void *);
void *avl_assert_delete (struct avl_table *, void *);

#define avl_count(table) ((size_t) (table)->avl_count)

/* Table traverser functions. */
void avl_t_init (struct avl_traverser *, struct avl_table *);
void *avl_t_first (struct avl_traverser *, struct avl_table *);
void *avl_t_last (struct avl_traverser *, struct avl_table *);
void *avl_t_find (struct avl_traverser *, struct avl_table *, void *);
void *avl_t_insert (struct avl_traverser *, struct avl_table *, void *);
void *avl_t_copy (struct avl_traverser *, const struct avl_traverser *);
void *avl_t_next (struct avl_traverser *);
void *avl_t_prev (struct avl_traverser *);
void *avl_t_cur (struct avl_traverser *);
void *avl_t_replace (struct avl_traverser *, void *);

static GCC_INLINE int avl_get_decommissioned_flag(struct avl_node * node)
{
    return ((signed char)(node->avl_mylink[1] & 0x1));
}

static GCC_INLINE void avl_set_decommissioned_flag(struct avl_node * node, int decommissioned_flag)
{
    node->avl_mylink[1] &= (~0x1);
    node->avl_mylink[1] |= (decommissioned_flag ? 0x1 : 0x0);
}

#endif /* avl.h */
