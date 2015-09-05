/* Copyright (c) 2012 Shlomi Fish
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
 * dbm_calc_derived.h - contains functions to calculate the derived states
 * from a certain position.
 *
 */
#ifndef FC_SOLVE_DBM_CALC_DERIVED_H
#define FC_SOLVE_DBM_CALC_DERIVED_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdlib.h>

#include "config.h"

#include "bool.h"
#include "inline.h"
#include "state.h"
#include "meta_alloc.h"
#include "fcs_enums.h"

#include "fcs_dllexport.h"
#include "dbm_common.h"
#include "dbm_solver_key.h"
#include "dbm_calc_derived_iface.h"
#include "indirect_buffer.h"
#include "fcc_brfs_test.h"

typedef struct fcs_derived_state_struct
{
    fcs_state_keyval_pair_t state;
    fcs_encoded_state_buffer_t key;
    fcs_dbm_record_t * parent;
    struct fcs_derived_state_struct * next;
    int core_irreversible_moves_count;
    fcs_which_moves_bitmask_t which_irreversible_moves_bitmask;
    fcs_fcc_move_t move;
    int num_non_reversible_moves_including_prune;
    DECLARE_IND_BUF_T(indirect_stacks_buffer)
} fcs_derived_state_t;

#define MAKE_MOVE(src, dest) ((src) | ((dest) << 4))
#define COL2MOVE(idx) (idx)
#define FREECELL2MOVE(idx) (idx+8)
#define FOUND2MOVE(idx) ((idx)+(8+4))

#define CALC_SEQUENCES_ARE_BUILT_BY() ( \
    (local_variant == FCS_DBM_VARIANT_BAKERS_DOZEN) \
    ? FCS_SEQ_BUILT_BY_RANK \
    : FCS_SEQ_BUILT_BY_ALTERNATE_COLOR \
    )

#ifdef INDIRECT_STACK_STATES

#define COPY_INDIRECT_COLS() \
{ \
    for (int copy_col_idx=0 ; copy_col_idx < LOCAL_STACKS_NUM ; copy_col_idx++) \
    { \
        copy_stack_col = fcs_state_get_col((ptr_new_state->state.s), copy_col_idx); \
        memcpy(&(ptr_new_state->indirect_stacks_buffer[copy_col_idx << 7]), copy_stack_col, fcs_col_len(copy_stack_col)+1); \
        fcs_state_get_col((ptr_new_state->state.s), copy_col_idx) = &(ptr_new_state->indirect_stacks_buffer[copy_col_idx << 7]); \
   } \
}

#else

#define COPY_INDIRECT_COLS() {}

#endif

#define BEGIN_NEW_STATE() \
{ \
    if (*derived_list_recycle_bin) \
    {  \
        (*derived_list_recycle_bin) = \
        (ptr_new_state =  \
         (*derived_list_recycle_bin) \
        )->next; \
    } \
    else \
    { \
        ptr_new_state = \
        (fcs_derived_state_t *) \
        fcs_compact_alloc_ptr( \
                derived_list_allocator, \
                sizeof(*ptr_new_state) \
                ); \
    } \
    memset( \
        &(ptr_new_state->which_irreversible_moves_bitmask), \
        '\0', \
        sizeof(ptr_new_state->which_irreversible_moves_bitmask) \
        ); \
    fcs_duplicate_state(  \
        &(ptr_new_state->state), \
        init_state_kv_ptr \
    ); \
        \
    COPY_INDIRECT_COLS() \
}

static GCC_INLINE void fc_solve_add_to_irrev_moves_bitmask(
    fcs_which_moves_bitmask_t * const which_irreversible_moves_bitmask,
    const fcs_card_t moved_card,
    const int count
)
{
    unsigned char * const by_rank_ptr = which_irreversible_moves_bitmask->s + fcs_card_rank(moved_card) - 1;
    const int suit_double = (fcs_card_suit(moved_card) << 1);
    const int new_count = ( (((*by_rank_ptr)>>(suit_double))&((1 << 2)-1)) + count );
    *by_rank_ptr &= (~((((unsigned char)0x3) << (suit_double))));
    *by_rank_ptr |= (new_count << (suit_double));

    return;
}

#define COMMIT_NEW_STATE_WITH_COUNT(src, dest, count, moved_card) \
{ \
 \
    if (count) \
    { \
        fc_solve_add_to_irrev_moves_bitmask(&(ptr_new_state->which_irreversible_moves_bitmask), moved_card, count); \
    } \
    ptr_new_state->parent = parent_ptr; \
    ptr_new_state->move = MAKE_MOVE((src), (dest)); \
 \
    ptr_new_state->core_irreversible_moves_count = (count); \
    /* Finally, enqueue the new state. */ \
    ptr_new_state->next = (*derived_list); \
    (*derived_list) = ptr_new_state; \
 \
}

#define COMMIT_NEW_STATE(src, dest, is_reversible, moved_card) \
    COMMIT_NEW_STATE_WITH_COUNT(src, dest, ((is_reversible) ? 0 : 1), moved_card)

#ifdef FCS_FREECELL_ONLY
#define SEQS_ARE_BUILT_BY_RANK() FALSE
#else
#define SEQS_ARE_BUILT_BY_RANK() (sequences_are_built_by == FCS_SEQ_BUILT_BY_RANK)
#endif

static GCC_INLINE int calc_foundation_to_put_card_on(
        enum fcs_dbm_variant_type_t local_variant,
        fcs_state_t * my_ptr_state,
        fcs_card_t card
        )
{
    int deck;

#ifndef FCS_FREECELL_ONLY
    /* needed by the macros. */
    const int sequences_are_built_by = CALC_SEQUENCES_ARE_BUILT_BY();
#endif

    for(deck=0;deck < INSTANCE_DECKS_NUM;deck++)
    {
        if (fcs_foundation_value(*my_ptr_state, (deck<<2)+fcs_card_suit(card)) == fcs_card_rank(card) - 1)
        {
            int other_deck_idx;

            for (other_deck_idx = 0 ; other_deck_idx < (INSTANCE_DECKS_NUM << 2) ; other_deck_idx++)
            {
                if (fcs_foundation_value(*my_ptr_state, other_deck_idx)
                        < fcs_card_rank(card) - 2 -
                        (
                            SEQS_ARE_BUILT_BY_RANK()
                            ? 0
                            : ((other_deck_idx&0x1) == (fcs_card_suit(card)&0x1))
                        )
                   )
                {
                    break;
                }
            }
            if (other_deck_idx == (INSTANCE_DECKS_NUM << 2))
            {
                return (deck<<2)+fcs_card_suit(card);
            }
        }
    }
    return -1;
}

typedef struct {
    fcs_fcc_moves_list_item_t * recycle_bin;
    fcs_compact_allocator_t * allocator;
} fcs_fcc_moves_seq_allocator_t;

static GCC_INLINE fcs_fcc_moves_list_item_t * fc_solve_fcc_alloc_moves_list_item(
    fcs_fcc_moves_seq_allocator_t * allocator
)
{
    fcs_fcc_moves_list_item_t * new_item;

    if (allocator->recycle_bin)
    {
        allocator->recycle_bin = (new_item = allocator->recycle_bin)->next;
    }
    else
    {
        new_item = (fcs_fcc_moves_list_item_t *)
            fcs_compact_alloc_ptr(
                allocator->allocator,
                sizeof(*new_item)
                );
    }
    new_item->next = NULL;

    return new_item;
}

#define FROM_COL_IS_REVERSIBLE_MOVE() \
                        ((cards_num <= 1) ? TRUE \
                             : fcs_is_parent_card(card, fcs_col_get_card( \
                                     col, cards_num-2) \
                               ) \
                        )


/* Returns the number of amortized irreversible moves performed. */
static GCC_INLINE int horne_prune(
    const enum fcs_dbm_variant_type_t local_variant,
    fcs_state_keyval_pair_t * const init_state_kv_ptr,
    fcs_which_moves_bitmask_t * const which_irreversible_moves_bitmask,
    fcs_fcc_moves_seq_t * const moves_seq,
    fcs_fcc_moves_seq_allocator_t * const allocator
)
{
    fcs_fcc_move_t additional_moves[RANK_KING * 4 * DECKS_NUM];
    int count_moves_so_far = 0;
    int count_additional_irrev_moves = 0;

#ifndef FCS_FREECELL_ONLY
    const int sequences_are_built_by = CALC_SEQUENCES_ARE_BUILT_BY();
#endif

#define the_state (init_state_kv_ptr->s)
    int num_cards_moved;
    do {
        num_cards_moved = 0;
        for ( int stack_idx = 0 ; stack_idx < LOCAL_STACKS_NUM ; stack_idx++ )
        {
            fcs_cards_column_t col = fcs_state_get_col(the_state, stack_idx);
            const int cards_num = fcs_col_len(col);
            if (cards_num)
            {
                /* Get the top card in the stack */
                const fcs_card_t card = fcs_col_get_card(col, cards_num-1);
                const int dest_foundation =
                    calc_foundation_to_put_card_on(
                        local_variant, &the_state, card
                    );
                if (dest_foundation >= 0)
                {
                    if (! FROM_COL_IS_REVERSIBLE_MOVE())
                    {
                        count_additional_irrev_moves++;
                    }
                    /* We can safely move it. */
                    num_cards_moved++;
                    fc_solve_add_to_irrev_moves_bitmask(
                        which_irreversible_moves_bitmask, card,
                        ((! FROM_COL_IS_REVERSIBLE_MOVE()) ? 2 : 1)
                    );

                    fcs_col_pop_top(col);

                    fcs_increment_foundation(the_state, dest_foundation);

                    additional_moves[count_moves_so_far++]
                        = MAKE_MOVE(COL2MOVE(stack_idx), FOUND2MOVE(dest_foundation));
                }
            }
        }

        /* Now check the same for the free cells */
        for ( int fc = 0 ; fc < LOCAL_FREECELLS_NUM ; fc++)
        {
            const fcs_card_t card = fcs_freecell_card(the_state, fc);
            if (fcs_card_is_valid(card))
            {
                const int dest_foundation =
                    calc_foundation_to_put_card_on(local_variant, &the_state, card);
                if (dest_foundation >= 0)
                {
                    num_cards_moved++;
                    fc_solve_add_to_irrev_moves_bitmask(
                        which_irreversible_moves_bitmask, card, 1
                    );

                    /* We can put it there */

                    fcs_empty_freecell(the_state, fc);
                    fcs_increment_foundation(the_state, dest_foundation);
                    additional_moves[count_moves_so_far++]
                        = MAKE_MOVE(COL2MOVE(fc), FOUND2MOVE(dest_foundation));
                }
            }
        }
    } while (num_cards_moved);

    /* modify moves_seq in-place. */
    if (count_moves_so_far && moves_seq)
    {
        fcs_fcc_moves_list_item_t * * iter = &(moves_seq->moves_list);

        /* Assuming FCS_FCC_NUM_MOVES_IN_ITEM is 8 and we want (*iter)
         * to point at the place to either write the new moves or alternatively
         * (on parity) on the pointer to allocate a new list_item for the
         * moves.
         *
         * If count is 0, then we should move 0.
         * If count is 1, then we should move 0.
         * .
         * .
         * .
         * If count is 7, then we should move 0.
         * If count is 8, then we should move 1 time.
         *
         * to sum up we need to move count / FCS_FCC_NUM_MOVES_IN_ITEM .
         *
         * */
        const int count = moves_seq->count;
        for (int pos = 0 ;
             pos <= count - FCS_FCC_NUM_MOVES_IN_ITEM ;
             pos += FCS_FCC_NUM_MOVES_IN_ITEM
        )
        {
            iter = &((*iter)->next);
        }

        int pos = count;

        for (int pos_moves_so_far = 0 ;
             pos_moves_so_far < count_moves_so_far ;
             pos_moves_so_far++)
        {
            if (pos % FCS_FCC_NUM_MOVES_IN_ITEM == 0)
            {
                (*iter) = fc_solve_fcc_alloc_moves_list_item(allocator);
            }
            (*iter)->data.s[pos % FCS_FCC_NUM_MOVES_IN_ITEM] = additional_moves[pos_moves_so_far];
            if ((++pos) % FCS_FCC_NUM_MOVES_IN_ITEM == 0)
            {
                iter = &((*iter)->next);
            }
        }
        moves_seq->count += count_moves_so_far;
    }

    return count_moves_so_far + count_additional_irrev_moves;
}

static GCC_INLINE fcs_bool_t instance_solver_thread_calc_derived_states(
    enum fcs_dbm_variant_type_t local_variant,
    fcs_state_keyval_pair_t * init_state_kv_ptr,
    fcs_dbm_record_t * parent_ptr,
    fcs_derived_state_t * * derived_list,
    fcs_derived_state_t * * derived_list_recycle_bin,
    fcs_compact_allocator_t * derived_list_allocator,
    const fcs_bool_t perform_horne_prune
)
{
    fcs_derived_state_t * ptr_new_state;
    int stack_idx, cards_num, ds;
    fcs_cards_column_t col, dest_col;
#ifdef INDIRECT_STACK_STATES
    fcs_cards_column_t copy_stack_col;
#endif
    fcs_card_t card, dest_card;
    int deck, suit;
    int empty_fc_idx = -1;
    int empty_stack_idx = -1;

#ifndef FCS_FREECELL_ONLY
    /* needed by the macros. */
    const int sequences_are_built_by = CALC_SEQUENCES_ARE_BUILT_BY();
#endif

#define the_state (init_state_kv_ptr->s)
#define new_state (ptr_new_state->state.s)

#define SUIT_LIMIT ( DECKS_NUM * 4 )
    for (suit = 0 ; suit < SUIT_LIMIT ; suit++)
    {
        if (fcs_foundation_value(the_state, suit) < RANK_KING)
        {
            break;
        }
    }

    if (suit == SUIT_LIMIT)
    {
        /* Solved state. */
        return TRUE;
    }

    /* Move top stack cards to foundations. */
    for (stack_idx=0 ; stack_idx < LOCAL_STACKS_NUM ; stack_idx++)
    {
        col = fcs_state_get_col(the_state, stack_idx);
        cards_num = fcs_col_len(col);
        if (cards_num)
        {
            /* Get the top card in the stack */
            card = fcs_col_get_card(col, cards_num-1);
            suit = fcs_card_suit(card);
            for (deck=0 ; deck < INSTANCE_DECKS_NUM ; deck++)
            {

                if (fcs_foundation_value(the_state, deck*4+suit) == fcs_card_rank(card) - 1)
                {
                    /* We can put it there */
                    BEGIN_NEW_STATE()

                    {
                        fcs_cards_column_t new_temp_col;
                        new_temp_col = fcs_state_get_col(new_state, stack_idx);
                        fcs_col_pop_top(new_temp_col);
                    }

                    fcs_increment_foundation(new_state, deck*4+suit);


                    COMMIT_NEW_STATE_WITH_COUNT(COL2MOVE(stack_idx), FOUND2MOVE(suit), (FROM_COL_IS_REVERSIBLE_MOVE() ? 1 : 2), card)
                }
            }
        }
        else
        {
            empty_stack_idx = stack_idx;
        }
    }

#define fc_idx stack_idx
    /* Move freecell cards to foundations. */
    for (fc_idx=0 ; fc_idx < LOCAL_FREECELLS_NUM ; fc_idx++)
    {
        card = fcs_freecell_card(the_state, fc_idx);
        suit = fcs_card_suit(card);
        if (fcs_card_is_valid(card))
        {
            for(deck=0;deck<INSTANCE_DECKS_NUM;deck++)
            {
                if (fcs_foundation_value(the_state, deck*4+suit) == fcs_card_rank(card) - 1)
                {
                    BEGIN_NEW_STATE()

                    /* We can put it there */
                    fcs_empty_freecell(new_state, fc_idx);

                    fcs_increment_foundation(new_state, deck*4+suit);

                    COMMIT_NEW_STATE(
                        FREECELL2MOVE(fc_idx), FOUND2MOVE(suit), FALSE, card
                    )
                }
            }
        }
        else
        {
            empty_fc_idx = fc_idx;
        }
    }

    const int cards_num_min_limit =
        ( (local_variant == FCS_DBM_VARIANT_BAKERS_DOZEN) ? 1 : 0 );

    /* Move stack card on top of a parent */
    for (stack_idx=0;stack_idx<LOCAL_STACKS_NUM;stack_idx++)
    {
        col = fcs_state_get_col(the_state, stack_idx);
        cards_num = fcs_col_len(col);
        if (cards_num > cards_num_min_limit)
        {
            card = fcs_col_get_card(col, cards_num-1);

            for (ds=0;ds<LOCAL_STACKS_NUM;ds++)
            {
                if (ds == stack_idx)
                {
                    continue;
                }

                dest_col = fcs_state_get_col(the_state, ds);

                if (fcs_col_len(dest_col) > 0)
                {
                    dest_card = fcs_col_get_card(dest_col,
                            fcs_col_len(dest_col)-1);
                    if (fcs_is_parent_card(card, dest_card))
                    {
                        /* Let's move it */
                        BEGIN_NEW_STATE()

                        {
                            fcs_cards_column_t new_src_col;
                            fcs_cards_column_t new_dest_col;

                            new_src_col = fcs_state_get_col(new_state, stack_idx);
                            new_dest_col = fcs_state_get_col(new_state, ds);

                            fcs_col_pop_top(new_src_col);
                            fcs_col_push_card(new_dest_col, card);
                        }

                        COMMIT_NEW_STATE(
                            COL2MOVE(stack_idx), COL2MOVE(ds),
                            FROM_COL_IS_REVERSIBLE_MOVE(),
                            card
                        )
                    }
                }
            }
        }
    }

    /* Move freecell card on top of a parent */
    for (fc_idx=0 ; fc_idx < LOCAL_FREECELLS_NUM ; fc_idx++)
    {
        card = fcs_freecell_card(the_state, fc_idx);
        if (fcs_card_is_valid(card))
        {
            for (ds=0;ds<LOCAL_STACKS_NUM;ds++)
            {
                dest_col = fcs_state_get_col(the_state, ds);

                if (fcs_col_len(dest_col) > 0)
                {
                    dest_card = fcs_col_get_card(dest_col,
                            fcs_col_len(dest_col)-1);
                    if (fcs_is_parent_card(card, dest_card))
                    {
                        /* Let's move it */
                        BEGIN_NEW_STATE()

                        {
                            fcs_cards_column_t new_dest_col;

                            new_dest_col = fcs_state_get_col(new_state, ds);

                            fcs_col_push_card(new_dest_col, card);

                            fcs_empty_freecell(new_state, fc_idx);
                        }

                        COMMIT_NEW_STATE(
                            FREECELL2MOVE(fc_idx), COL2MOVE(ds),
                            TRUE,
                            card
                        )
                    }
                }
            }
        }
    }

    if ((local_variant != FCS_DBM_VARIANT_BAKERS_DOZEN)
        && (empty_stack_idx >= 0))
    {
        /* Stack Card to Empty Stack */
        for(stack_idx=0;stack_idx<LOCAL_STACKS_NUM;stack_idx++)
        {
            col = fcs_state_get_col(the_state, stack_idx);
            cards_num = fcs_col_len(col);
            /* Bug fix: if there's only one card in a column, there's no
             * point moving it to a new empty column.
             * */
            if (cards_num > 1)
            {
                card = fcs_col_get_card(col, cards_num-1);
                /* Let's move it */
                {
                    BEGIN_NEW_STATE()

                    {
                        fcs_cards_column_t new_src_col;
                        fcs_cards_column_t empty_stack_col;

                        new_src_col = fcs_state_get_col(new_state, stack_idx);

                        fcs_col_pop_top(new_src_col);

                        empty_stack_col = fcs_state_get_col(new_state, empty_stack_idx);
                        fcs_col_push_card(empty_stack_col, card);
                    }
                    COMMIT_NEW_STATE(
                        COL2MOVE(stack_idx), COL2MOVE(empty_stack_idx),
                        FROM_COL_IS_REVERSIBLE_MOVE(),
                        card
                    )
                }
            }
        }

        /* Freecell card -> Empty Stack. */
        for (fc_idx=0;fc_idx<LOCAL_FREECELLS_NUM;fc_idx++)
        {
            card = fcs_freecell_card(the_state, fc_idx);
            if (fcs_card_is_empty(card))
            {
                continue;
            }

            {
                BEGIN_NEW_STATE()

                {
                    fcs_cards_column_t new_dest_col;
                    new_dest_col = fcs_state_get_col(new_state, empty_stack_idx);
                    fcs_col_push_card(new_dest_col, card);
                    fcs_empty_freecell(new_state, fc_idx);
                }

                COMMIT_NEW_STATE(
                    FREECELL2MOVE(fc_idx), COL2MOVE(empty_stack_idx),
                    TRUE, card
                );
            }
        }
    }

    if (empty_fc_idx >= 0)
    {
        /* Stack Card to Empty Freecell */
        for (stack_idx=0;stack_idx<LOCAL_STACKS_NUM;stack_idx++)
        {
            col = fcs_state_get_col(the_state, stack_idx);
            cards_num = fcs_col_len(col);
            if (cards_num > cards_num_min_limit)
            {
                card = fcs_col_get_card(col, cards_num-1);
                /* Let's move it */
                {
                    BEGIN_NEW_STATE()

                    {
                        fcs_cards_column_t new_src_col;

                        new_src_col = fcs_state_get_col(new_state, stack_idx);

                        fcs_col_pop_top(new_src_col);

                        fcs_put_card_in_freecell(new_state, empty_fc_idx, card);
                    }
                    COMMIT_NEW_STATE(
                        COL2MOVE(stack_idx), FREECELL2MOVE(empty_fc_idx),
                        FROM_COL_IS_REVERSIBLE_MOVE(),
                        card
                    )
                }
            }
        }
    }
#undef fc_idx

    /* Perform Horne's Prune on all the states,
     * or just set their num irreversible moves counts.
     * */
    {
        fcs_derived_state_t * derived_iter;

        for (derived_iter = (*derived_list);
             derived_iter ;
             derived_iter = derived_iter->next
            )
        {
            derived_iter->num_non_reversible_moves_including_prune =
            (
                derived_iter->core_irreversible_moves_count
                +
                (
                    perform_horne_prune
                    ? horne_prune(local_variant, &(derived_iter->state), &(derived_iter->which_irreversible_moves_bitmask), NULL, NULL)
                    : 0
                )
            );
        }
    }

    return FALSE;
}

#undef FROM_COL_IS_REVERSIBLE_MOVE
#undef the_state
#undef new_state


#ifdef __cplusplus
}
#endif

#endif /*  FC_SOLVE_DBM_CALC_DERIVED_H */
