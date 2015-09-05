/* Copyright (c) 2000 Shlomi Fish
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 * meta_alloc.h - the Freecell Solver compact allocator based on the
 * meta-allocator concept that is used to collect the pages allocated by
 * the standard allocator after it is destroyed and to recycle them.
 *
 * Also see alloc.h.
 */
#ifndef FC_SOLVE__META_ALLOC_H
#define FC_SOLVE__META_ALLOC_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "state.h"

#include "inline.h"

typedef struct
{
    char * recycle_bin;
} fcs_meta_compact_allocator_t;

typedef struct
{
    char * old_list;
    char * max_ptr;
    char * ptr;
    char * rollback_ptr;
    fcs_meta_compact_allocator_t * meta;
} fcs_compact_allocator_t;

extern void fc_solve_compact_allocator_extend(
    fcs_compact_allocator_t * const allocator
        );

/* To be called after the meta_alloc was set. */
static GCC_INLINE void fc_solve_compact_allocator_init_helper(
    fcs_compact_allocator_t * const allocator
)
{
    allocator->old_list = NULL;
    fc_solve_compact_allocator_extend(allocator);
}

static GCC_INLINE void fc_solve_meta_compact_allocator_init(
    fcs_meta_compact_allocator_t * const meta
    )
{
    meta->recycle_bin = NULL;
}

extern void fc_solve_meta_compact_allocator_finish(
    fcs_meta_compact_allocator_t * const meta_allocator
    );

extern void fc_solve_compact_allocator_init(
    fcs_compact_allocator_t * const allocator,
    fcs_meta_compact_allocator_t * const meta_allocator
    );


static GCC_INLINE void * fcs_compact_alloc_ptr(
    fcs_compact_allocator_t * const allocator,
    const int how_much_proto
)
{
    /* Round ptr to the next pointer boundary */
    const int how_much = how_much_proto +
        (
         (sizeof(char *)-((how_much_proto)&(sizeof(char *)-1)))&(sizeof(char*)-1)
        );

    if (allocator->max_ptr - allocator->ptr < how_much)
    {
        fc_solve_compact_allocator_extend(allocator);
    }
    else
    {
        allocator->rollback_ptr = allocator->ptr;
    }
    allocator->ptr += how_much;

    return allocator->rollback_ptr;
}

#define fcs_compact_alloc_release(allocator) \
{    \
    (allocator)->ptr = (allocator)->rollback_ptr; \
}

extern void fc_solve_compact_allocator_finish(
    fcs_compact_allocator_t * const allocator
);

static GCC_INLINE fcs_collectible_state_t * fcs_state_ia_alloc_into_var(fcs_compact_allocator_t * const allocator)
{
    return
        (fcs_collectible_state_t *)
        fcs_compact_alloc_ptr(allocator,
            sizeof(fcs_collectible_state_t)
        );
}

static GCC_INLINE void fc_solve_compact_allocator_recycle(fcs_compact_allocator_t * const allocator)
{
    fc_solve_compact_allocator_finish(allocator);
    fc_solve_compact_allocator_init_helper(allocator);
}

#ifdef __cplusplus
};
#endif

#endif /* FC_SOLVE__META_ALLOC_H */
