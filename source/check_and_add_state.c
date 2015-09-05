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
 * check_and_add_state.c - the various possible implementations of the function
 * fc_solve_check_and_add_state().
 *
 */

#define BUILDING_DLL 1

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "fcs_dm.h"
#include "instance.h"

#include "move_stack_compact_alloc.h"
#include "inline.h"
#include "likely.h"

typedef  unsigned long  int  ub4;   /* unsigned 4-byte quantities */
typedef  unsigned       char ub1;

static GCC_INLINE ub4 perl_hash_function(
    register const ub1 *s_ptr,        /* the key */
    register const ub4  length        /* the length of the key */
    )
{
    register ub4  hash_value_int = 0;
    register const ub1 * const s_end = s_ptr+length;

    while (s_ptr < s_end)
    {
        hash_value_int += (hash_value_int << 5) + *(s_ptr++);
    }
    hash_value_int += (hash_value_int>>5);

    return hash_value_int;
}

#ifdef INDIRECT_STACK_STATES

#define replace_with_cached(condition_expr) \
        if (condition_expr)     \
        {      \
            fcs_compact_alloc_release(stacks_allocator);    \
            *(current_stack) = cached_stack;       \
        }

/* TODO : Maybe define an accesor for new_state_key->stacks (also see the
 * replaced_with_cached macro above.
 * */
static GCC_INLINE void fc_solve_cache_stacks(
        fc_solve_hard_thread_t * const hard_thread,
        fcs_kv_state_t * const new_state
        )
{
#ifdef FCS_SINGLE_HARD_THREAD
#define instance hard_thread
#else
    fc_solve_instance_t * const instance = hard_thread->instance;
#endif

#ifndef HARD_CODED_NUM_STACKS
    SET_GAME_PARAMS();
#endif
    register fcs_state_t * const new_state_key = new_state->key;
    register fcs_state_extra_info_t * const new_state_info = new_state->val;

    fcs_compact_allocator_t * const stacks_allocator = &(HT_FIELD(hard_thread, allocator));

    fcs_cards_column_t * current_stack = new_state_key->stacks;

    for (int i=0 ; i < LOCAL_STACKS_NUM ; i++, current_stack++)
    {
        /*
         * If the stack is not a copy - it is already cached so skip
         * to the next stack
         * */
        if (! (new_state_info->stacks_copy_on_write_flags & (1 << i)))
        {
            continue;
        }

        fcs_cards_column_t column = fcs_state_get_col(*(new_state_key), i);
        const int col_len = (fcs_col_len(column)+1);

        char * const new_ptr = (char*)fcs_compact_alloc_ptr(stacks_allocator, col_len);
        memcpy(new_ptr, column, col_len);
        *(current_stack) = new_ptr;

        void * cached_stack;
#if FCS_STACK_STORAGE == FCS_STACK_STORAGE_INTERNAL_HASH
#ifdef FCS_ENABLE_SECONDARY_HASH_VALUE
        /* Calculate the hash value for the stack */
        /* This hash function was ripped from the Perl source code.
         * (It is not derived work however). */
        fc_solve_hash_value_t hash_value_int = 0;
        {
            const char * s_ptr = (char*)(*(current_stack));
            const char * s_end = s_ptr+fcs_col_len(s_ptr)+1;
            while (s_ptr < s_end)
            {
                hash_value_int += (hash_value_int << 5) + *(s_ptr++);
            }
            hash_value_int += (hash_value_int >> 5);
        }

        if (hash_value_int < 0)
        {
            /*
             * This is a bit mask that nullifies the sign bit of the
             * number so it will always be positive
             * */
            hash_value_int &= (~(1<<((sizeof(hash_value_int)<<3)-1)));
        }
#endif

        {
            column = fcs_state_get_col(*(new_state_key), i);

            replace_with_cached(fc_solve_hash_insert(
                &(instance->stacks_hash),
                column,
                &cached_stack,
                perl_hash_function(
                    (ub1 *)*(current_stack),
                    col_len
                    )
#ifdef FCS_ENABLE_SECONDARY_HASH_VALUE
                , hash_value_int
#endif
                )
            );
        }

#elif (FCS_STACK_STORAGE == FCS_STACK_STORAGE_GOOGLE_DENSE_HASH)
        {
            column = fcs_state_get_col(*new_state_key, i);

            replace_with_cached(
                fc_solve_columns_google_hash_insert(
                    instance->stacks_hash,
                    column,
                    &cached_stack
                )
            );
        }
#elif (FCS_STACK_STORAGE == FCS_STACK_STORAGE_LIBAVL2_TREE)

        cached_stack =
            fcs_libavl2_stacks_tree_insert(
                instance->stacks_tree,
                new_state_key->stacks[i]
            );

        replace_with_cached(cached_stack != NULL);

#elif (FCS_STACK_STORAGE == FCS_STACK_STORAGE_LIBREDBLACK_TREE)
        cached_stack = (void *)rbsearch(
            *(current_stack),
            instance->stacks_tree
            );

        replace_with_cached(cached_stack != *(current_stack));
#elif (FCS_STACK_STORAGE == FCS_STACK_STORAGE_GLIB_TREE)
        cached_stack = g_tree_lookup(
             instance->stacks_tree,
             (gpointer)*(current_stack)
             );

        /* replace_with_cached contains an if statement */
        replace_with_cached(cached_stack != NULL)
        else
        {
            g_tree_insert(
                instance->stacks_tree,
                (gpointer)*(current_stack),
                (gpointer)*(current_stack)
                );
        }
#elif (FCS_STACK_STORAGE == FCS_STACK_STORAGE_GLIB_HASH)
        cached_stack = g_hash_table_lookup(
            instance->stacks_hash,
            (gconstpointer)*(current_stack)
            );
        replace_with_cached(cached_stack != NULL)
        else
        {
            g_hash_table_insert(
                instance->stacks_hash,
                (gpointer)*(current_stack),
                (gpointer)*(current_stack)
                );
        }
#elif (FCS_STACK_STORAGE == FCS_STACK_STORAGE_JUDY)
        PWord_t * PValue;
        column = fcs_state_get_col(*new_state_key, i);

        JHSI(
            PValue,
            instance->stacks_judy_array,
            column,
            (1+fcs_col_len(column))
        );
        /* later_todo : Handle out-of-memory. */
        if (*PValue == 0)
        {
            /*  A new stack */
            *PValue = (PWord_t)column;
        }
        else
        {
            cached_stack = (void *)(*PValue);
            replace_with_cached(1);
        }
#else
#error FCS_STACK_STORAGE is not set to a good value.
#endif
    }
#ifdef FCS_SINGLE_HARD_THREAD
#undef instance
#endif
}

#else /* #ifdef INDIRECT_STACK_STATES */

#define fc_solve_cache_stacks(hard_thread, new_state_key) \
    {}

#endif

#if (FCS_STATE_STORAGE == FCS_STATE_STORAGE_GLIB_HASH)
guint fc_solve_hash_function(gconstpointer key)
{
    const char * s_ptr = (char*)key;
    const char * const s_end = s_ptr+sizeof(fcs_state_t);
    guint hash_value = 0;
    while (s_ptr < s_end)
    {
        hash_value += (hash_value << 5) + *(s_ptr++);
    }
    hash_value += (hash_value >> 5);

    return hash_value;
}
#endif


/*
 * check_and_add_state() does the following things:
 *
 * 1. Check if the number of iterations exceeded its maximum, and if so
 *    return FCS_STATE_EXCEEDS_MAX_NUM_TIMES in order to terminate the
 *    solving process.
 * 2. Check if the maximal depth was reached and if so return
 *    FCS_STATE_EXCEEDS_MAX_DEPTH
 * 3. Canonize the state.
 * 4. Check if the state is already found in the collection of the states
 *    that were already checked.
 *    If it is:
 *
 *        5a. Return FALSE.
 *
 *    If it isn't:
 *
 *        5b. Add the new state and return TRUE.
 * */

static GCC_INLINE void upon_new_state(
    fc_solve_instance_t * const instance,
    fc_solve_hard_thread_t * const hard_thread,
    fcs_state_extra_info_t * const new_state_info
)
{
    fcs_collectible_state_t * const parent_state = new_state_info->parent;
    /* The new state was not found in the cache, and it was already inserted */
    if (likely(parent_state))
    {
        (FCS_S_NUM_ACTIVE_CHILDREN(parent_state))++;
        /* If parent_val is defined, so is moves_to_parent */
        new_state_info->moves_to_parent =
            fc_solve_move_stack_compact_allocate(
                hard_thread,
                new_state_info->moves_to_parent
            );
    }

    instance->active_num_states_in_collection++;
    instance->num_states_in_collection++;

    return;
}

fcs_bool_t fc_solve_check_and_add_state(
    fc_solve_hard_thread_t * const hard_thread,
    fcs_kv_state_t * const new_state,
    fcs_kv_state_t * const existing_state_raw
    )
{
/*
 * TODO : these accessor macros are probably out-of-date and won't work with
 * some of the less commonly tested data storage backends. They should be
 * tested and updated.
 * */
#define existing_state_val (existing_state_raw->val)
#define new_state_key      (new_state->key)

#define ON_STATE_NEW() upon_new_state(instance, hard_thread, new_state->val);

#ifdef FCS_SINGLE_HARD_THREAD
#define instance hard_thread
#else
    fc_solve_instance_t * const instance = hard_thread->instance;
#endif

    /* #if'ing out because it doesn't belong here. */
#if 0
    if ((instance->max_depth >= 0) &&
        (new_state_val->depth >= instance->max_depth))
    {
        return FCS_STATE_EXCEEDS_MAX_DEPTH;
    }
#endif

    fc_solve_cache_stacks(hard_thread, new_state);

    {
        fc_solve_canonize_state(
            new_state,
            INSTANCE_FREECELLS_NUM,
            INSTANCE_STACKS_NUM
            );

    }

/*
    The objective of this part of the code is:
    1. To check if new_state_key / new_state_val is already in the prev_states
       collection.
    2. If not, to add it and to set check to true.
    3. If so, to set check to false.
  */

#if (FCS_STATE_STORAGE == FCS_STATE_STORAGE_INTERNAL_HASH)
#ifdef FCS_ENABLE_SECONDARY_HASH_VALUE
    fc_solve_hash_value_t hash_value_int = 0;
    {
        const char * s_ptr = (char*)new_state_key;
        const char * s_end = s_ptr+sizeof(*new_state_key);
        while (s_ptr < s_end)
        {
            hash_value_int += (hash_value_int << 5) + *(s_ptr++);
        }
        hash_value_int += (hash_value_int>>5);
    }
    if (hash_value_int < 0)
    {
        /*
         * This is a bit mask that nullifies the sign bit of the
         * number so it will always be positive
         * */
        hash_value_int &= (~(1<<((sizeof(hash_value_int)<<3)-1)));
    }
#endif
    {
        void * existing_void;
        if (fc_solve_hash_insert(
        &(instance->hash),
#ifdef FCS_RCS_STATES
        new_state->val,
        new_state->key,
#else
        FCS_STATE_kv_to_collectible(new_state),
#endif
        &existing_void,
        perl_hash_function(
            (ub1 *)(new_state_key),
            sizeof(*(new_state_key))
            )
#ifdef FCS_ENABLE_SECONDARY_HASH_VALUE
        , hash_value_int
#endif
        ))
        {
            FCS_STATE_collectible_to_kv(existing_state_raw, existing_void);
            return FALSE;
        }
        else
        {
            ON_STATE_NEW();
            return TRUE;
        }
    }
#elif (FCS_STATE_STORAGE == FCS_STATE_STORAGE_GOOGLE_DENSE_HASH)
    {
        void * existing_void;

        /*  TODO : check if this condition should be negated. */
        if (fc_solve_states_google_hash_insert(
            instance->hash,
            new_state,
            &(existing_void)
            )
        )
        {
            existing_state_val = existing_void;
            return FALSE;
        }
        else
        {
            ON_STATE_NEW();
            return TRUE;
        }
    }
#elif (FCS_STATE_STORAGE == FCS_STATE_STORAGE_INDIRECT)
    fcs_standalone_state_ptrs_t * pos_ptr;
    /* Try to see if the state is found in indirect_prev_states */
    if ((pos_ptr = (fcs_standalone_state_ptrs_t *)bsearch(&new_state_key,
                instance->indirect_prev_states,
                instance->num_indirect_prev_states,
                sizeof(instance->indirect_prev_states[0]),
                fc_solve_state_compare_indirect)) == NULL)
    {
        fcs_bool_t found;
        /* It isn't in prev_states, but maybe it's in the sort margin */
        pos_ptr = (fcs_standalone_state_ptrs_t *)fc_solve_bsearch(
            &new_state_key,
            instance->indirect_prev_states_margin,
            instance->num_prev_states_margin,
            sizeof(instance->indirect_prev_states_margin[0]),
            fc_solve_state_compare_indirect_with_context,
            NULL,
            &found);

        if (found)
        {
            is_state_new = FALSE;
            existing_state_val = pos_ptr->val;
        }
        else
        {
            /* Insert the state into its corresponding place in the sort
             * margin */
            memmove((void*)(pos_ptr+1),
                    (void*)pos_ptr,
                    sizeof(*pos_ptr) *
                    (instance->num_prev_states_margin-
                      (pos_ptr-instance->indirect_prev_states_margin)
                    ));
            pos_ptr->key = new_state_key;
            pos_ptr->val = new_state_val;

            instance->num_prev_states_margin++;

            if (instance->num_prev_states_margin >= PREV_STATES_SORT_MARGIN)
            {
                /* The sort margin is full, let's combine it with the main array */

                instance->indirect_prev_states =
                    SREALLOC(
                        instance->indirect_prev_states,
                        instance->num_indirect_prev_states
                            + instance->num_prev_states_margin
                    );

                fc_solve_merge_large_and_small_sorted_arrays(
                    instance->indirect_prev_states,
                    instance->num_indirect_prev_states,
                    instance->indirect_prev_states_margin,
                    instance->num_prev_states_margin,
                    sizeof(instance->indirect_prev_states[0]),
                    fc_solve_state_compare_indirect_with_context,
                    NULL
                );

                instance->num_indirect_prev_states += instance->num_prev_states_margin;

                instance->num_prev_states_margin=0;
            }
            is_state_new = TRUE;
        }

    }
    else
    {
        existing_state_val = pos_ptr->val;
        is_state_new = FALSE;
    }

#elif (FCS_STATE_STORAGE == FCS_STATE_STORAGE_LIBREDBLACK_TREE)
    existing_state_val = (fcs_state_extra_info_t *)rbsearch(new_state_val,
            instance->tree
            );
    is_state_new = ((*existing_state_val) == new_state_val);

#elif (FCS_STATE_STORAGE == FCS_STATE_STORAGE_KAZ_TREE)
#ifdef FCS_RCS_STATES
    instance->tree_new_state_key = new_state->key;
    instance->tree_new_state = new_state->val;
#endif

    {
        void * existing_void;
    if ((existing_void =
        fc_solve_kaz_tree_alloc_insert(instance->tree,
#ifdef FCS_RCS_STATES
                                       new_state_val
#else
                                       FCS_STATE_kv_to_collectible(new_state)
#endif
                                       ))
            == NULL
       )
    {
        ON_STATE_NEW();
        return TRUE;
    }
    else
    {
        FCS_STATE_collectible_to_kv(existing_state_raw, existing_void);
        return FALSE;
    }
    }
#elif (FCS_STATE_STORAGE == FCS_STATE_STORAGE_LIBAVL2_TREE)
#ifdef FCS_RCS_STATES
    instance->tree_new_state_key = new_state->key;
    instance->tree_new_state = new_state->val;
#endif

    {
        void * existing_void;
    if ((existing_void =
        fcs_libavl2_states_tree_insert(instance->tree, FCS_STATE_kv_to_collectible(new_state)))
            == NULL
       )
    {
        ON_STATE_NEW();
        return TRUE;
    }
    else
    {
        FCS_STATE_collectible_to_kv(existing_state_raw, existing_void);
        return FALSE;
    }
    }
#elif (FCS_STATE_STORAGE == FCS_STATE_STORAGE_GLIB_TREE)
    existing_state_val = g_tree_lookup(instance->tree, (gpointer)new_state_key);
    if ((is_state_new = (existing_state_val == NULL)))
    {
        /* The new state was not found. Let's insert it.
         * The value must be the same as the key, so g_tree_lookup()
         * will return it. */
        g_tree_insert(
            instance->tree,
            (gpointer)new_state_key,
            (gpointer)new_state_val
            );
    }

#elif (FCS_STATE_STORAGE == FCS_STATE_STORAGE_GLIB_HASH)
    existing_state_val = g_hash_table_lookup(instance->hash,
            (gpointer)new_state_key);
    if ((is_state_new = (existing_state_val == NULL)))
    {
        /* The new state was not found. Let's insert it.
         * The value must be the same as the key, so g_tree_lookup()
         * will return it. */
        g_hash_table_insert(
            instance->hash,
            (gpointer)new_state_key,
            (gpointer)new_state_val

            );
    }

#elif (FCS_STATE_STORAGE == FCS_STATE_STORAGE_DB_FILE)
    {
        DBT key, value;
        key.data = new_state;
        key.size = sizeof(*new_state);
        if ((is_state_new = (instance->db->get(
            instance->db,
            NULL,
            &key,
            &value,
            0
            ) == 0)))
        {
            /* The new state was not found. Let's insert it.
             * The value must be the same as the key, so g_tree_lookup()
             * will return it. */

            value.data = key.data;
            value.size = key.size;
            instance->db->put(
                instance->db,
                NULL,
                &key,
                &value,
                0);
        }
    }
#elif (FCS_STATE_STORAGE == FCS_STATE_STORAGE_JUDY)
    {
        PWord_t * PValue;

        JHSI(PValue, instance->judy_array, new_state_key, sizeof(*new_state_key));

        /* later_todo : Handle out-of-memory. */
        if (*PValue == 0)
        {
            /* A new state. */
            *PValue = (PWord_t)(FCS_STATE_kv_to_collectible(new_state));
            ON_STATE_NEW();
            return TRUE;
        }
        else
        {
            /* Already exists. */
            FCS_STATE_collectible_to_kv(existing_state_raw, (fcs_collectible_state_t *)(*PValue));
            return FALSE;
        }
    }
#else
#error Unknown FCS_STATE_STORAGE. Please define it to a valid value.
#endif
}
#ifdef FCS_SINGLE_HARD_THREAD
#undef instance
#endif
