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
 * move.c - move and move stacks routines for Freecell Solver
 *
 */

#define BUILDING_DLL 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "move.h"
#include "state.h"
#include "fcs_enums.h"

#include "inline.h"
#include "unused.h"

#ifdef FCS_USE_COMPACT_MOVE_TOKENS
const fcs_internal_move_t fc_solve_empty_move = {0,0,0,0};
#else
const fcs_internal_move_t fc_solve_empty_move = {"\0\0\0\0"};
#endif

/*
 * This function performs a given move on a state
 */
void fc_solve_apply_move(
    fcs_kv_state_t * state,
    fcs_state_locs_struct_t * locs,
    fcs_internal_move_t move,
    int freecells_num,
    int stacks_num,
    int decks_num GCC_UNUSED
)
{
    fcs_card_t card;
    fcs_cards_column_t col;

#define state_key (state->key)
    switch(fcs_int_move_get_type(move))
    {
        case FCS_MOVE_TYPE_STACK_TO_STACK:
        {
            fcs_cards_column_t dest_col;
            int i;

            col = fcs_state_get_col(*state_key, fcs_int_move_get_src_stack(move));
            dest_col = fcs_state_get_col(*state_key, fcs_int_move_get_dest_stack(move));
            for(i=0 ; i<fcs_int_move_get_num_cards_in_seq(move) ; i++)
            {
                fcs_col_push_col_card(
                    dest_col,
                    col,
                    fcs_col_len(col) - fcs_int_move_get_num_cards_in_seq(move)+i
                );
            }
            for(i=0 ; i<fcs_int_move_get_num_cards_in_seq(move) ; i++)
            {
                fcs_col_pop_top(col);
            }
        }
        break;
        case FCS_MOVE_TYPE_FREECELL_TO_STACK:
        {
            col = fcs_state_get_col(*state_key, fcs_int_move_get_dest_stack(move));
            fcs_col_push_card(col, fcs_freecell_card(*state_key, fcs_int_move_get_src_freecell(move)));
            fcs_empty_freecell(*state_key, fcs_int_move_get_src_freecell(move));
        }
        break;
        case FCS_MOVE_TYPE_FREECELL_TO_FREECELL:
        {
            card = fcs_freecell_card(*state_key, fcs_int_move_get_src_freecell(move));
            fcs_put_card_in_freecell(*state_key, fcs_int_move_get_dest_freecell(move), card);
            fcs_empty_freecell(*state_key, fcs_int_move_get_src_freecell(move));
        }
        break;

        case FCS_MOVE_TYPE_STACK_TO_FREECELL:
        {
            col = fcs_state_get_col(*state_key, fcs_int_move_get_src_stack(move));
            fcs_col_pop_card(col, card);
            fcs_put_card_in_freecell(*state_key, fcs_int_move_get_dest_freecell(move), card);
        }
        break;

        case FCS_MOVE_TYPE_STACK_TO_FOUNDATION:
        {
            col = fcs_state_get_col(
                *state_key,
                fcs_int_move_get_src_stack(move)
                );
            fcs_col_pop_top(col);
            fcs_increment_foundation(*state_key, fcs_int_move_get_foundation(move));
        }
        break;

        case FCS_MOVE_TYPE_FREECELL_TO_FOUNDATION:
        {
            fcs_empty_freecell(*state_key, fcs_int_move_get_src_freecell(move));
            fcs_increment_foundation(*state_key, fcs_int_move_get_foundation(move));
        }
        break;

        case FCS_MOVE_TYPE_SEQ_TO_FOUNDATION:
        {
            int i;

            col = fcs_state_get_col(*state_key, fcs_int_move_get_src_stack(move));
            for (i=0 ; i<13 ; i++)
            {
                fcs_col_pop_top(col);
                fcs_increment_foundation(*state_key, fcs_int_move_get_foundation(move));
            }
        }
        break;

#ifndef FCS_WITHOUT_CARD_FLIPPING
        case FCS_MOVE_TYPE_FLIP_CARD:
        {
            col = fcs_state_get_col(*state_key, fcs_int_move_get_src_stack(move));
            fcs_col_flip_card(col, fcs_col_len(col)-1);
        }
        break;
#endif

        case FCS_MOVE_TYPE_CANONIZE:
        {
            if (locs)
            {
                fc_solve_canonize_state_with_locs(
                    state,
                    locs,
                    freecells_num, stacks_num
                );
            }
            else
            {
                fc_solve_canonize_state (state, freecells_num, stacks_num);
            }
        }
        break;

    }
#undef state_key
}

static GCC_INLINE int convert_freecell_num(int fcn)
{
    if (fcn >= 7)
        return (fcn+3);
    else
        return fcn;
}



char * fc_solve_move_to_string_w_state(
        fcs_state_keyval_pair_t * const state,
        const int freecells_num GCC_UNUSED,
        const int stacks_num GCC_UNUSED,
        const int decks_num GCC_UNUSED,
        const fcs_move_t move,
        const int standard_notation
        )
{
    char string[256];

#define state_key (&(state->s))
    switch(fcs_move_get_type(move))
    {
        case FCS_MOVE_TYPE_STACK_TO_STACK:
            if ((standard_notation == FC_SOLVE__STANDARD_NOTATION_EXTENDED) &&
                /* More than one card was moved */
                (fcs_move_get_num_cards_in_seq(move) > 1) &&
                /* It was a move to an empty stack */
                (fcs_col_len(fcs_state_get_col(*state_key, fcs_move_get_dest_stack(move))) ==
                 fcs_move_get_num_cards_in_seq(move))
               )
            {
                sprintf(string, "%i%iv%x",
                    1+fcs_move_get_src_stack(move),
                    1+fcs_move_get_dest_stack(move),
                    fcs_move_get_num_cards_in_seq(move)
                   );
            }
            else if (standard_notation)
            {
                sprintf(string, "%i%i",
                    1+fcs_move_get_src_stack(move),
                    1+fcs_move_get_dest_stack(move)
                    );
            }
            else
            {
                sprintf(string, "Move %i cards from stack %i to stack %i",
                    fcs_move_get_num_cards_in_seq(move),
                    fcs_move_get_src_stack(move),
                    fcs_move_get_dest_stack(move)
                );
            }
        break;

        case FCS_MOVE_TYPE_FREECELL_TO_STACK:
            if (standard_notation)
            {
                sprintf(string, "%c%i",
                    ('a'+convert_freecell_num(fcs_move_get_src_freecell(move))),
                    1+fcs_move_get_dest_stack(move)
                    );
            }
            else
            {
                sprintf(string, "Move a card from freecell %i to stack %i",
                    fcs_move_get_src_freecell(move),
                    fcs_move_get_dest_stack(move)
                    );
            }

        break;

        case FCS_MOVE_TYPE_FREECELL_TO_FREECELL:
            if (standard_notation)
            {
                sprintf(string, "%c%c",
( (char)('a'+(char)convert_freecell_num(fcs_move_get_src_freecell(move))) ),
( (char)('a'+(char)convert_freecell_num(fcs_move_get_dest_freecell(move))) )
                );
            }
            else
            {
                sprintf(string, "Move a card from freecell %i to freecell %i",
                    fcs_move_get_src_freecell(move),
                    fcs_move_get_dest_freecell(move)
                    );
            }

        break;

        case FCS_MOVE_TYPE_STACK_TO_FREECELL:
            if (standard_notation)
            {
                sprintf(string, "%i%c",
                    1+fcs_move_get_src_stack(move),
                    ('a'+convert_freecell_num(fcs_move_get_dest_freecell(move)))
                    );
            }
            else
            {
                sprintf(string, "Move a card from stack %i to freecell %i",
                    fcs_move_get_src_stack(move),
                    fcs_move_get_dest_freecell(move)
                    );
            }

        break;

        case FCS_MOVE_TYPE_STACK_TO_FOUNDATION:
            if (standard_notation)
            {
                sprintf(string, "%ih", 1+fcs_move_get_src_stack(move));
            }
            else
            {
                sprintf(string, "Move a card from stack %i to the foundations",
                    fcs_move_get_src_stack(move)
                    );
            }

        break;


        case FCS_MOVE_TYPE_FREECELL_TO_FOUNDATION:
            if (standard_notation)
            {
                sprintf(string, "%ch", ('a'+convert_freecell_num(fcs_move_get_src_freecell(move))));
            }
            else
            {
                sprintf(string,
                    "Move a card from freecell %i to the foundations",
                    fcs_move_get_src_freecell(move)
                    );
            }

        break;

        case FCS_MOVE_TYPE_SEQ_TO_FOUNDATION:
            if (standard_notation)
            {
                sprintf(string, "%ih", fcs_move_get_src_stack(move));
            }
            else
            {
                sprintf(string,
                    "Move the sequence on top of Stack %i to the foundations",
                    fcs_move_get_src_stack(move)
                    );
            }
        break;

        default:
            string[0] = '\0';
        break;
    }
#undef state_key

    return strdup(string);
}

#define DERIVED_STATES_LIST_GROW_BY 16
void fc_solve_derived_states_list_add_state(
        fcs_derived_states_list_t * list,
        fcs_collectible_state_t * state,
        int context
        )
{
    if (
        (!(
           (list->num_states+(list->states != NULL))
           & (DERIVED_STATES_LIST_GROW_BY-1)
          )
        )
       )
    {
        (list)->states = SREALLOC(
            (list)->states,
            list->num_states
            + (list->states!=NULL)
            + DERIVED_STATES_LIST_GROW_BY
        );
    }
    (list)->states[(list)->num_states].state_ptr = state;
    (list)->states[(list)->num_states++].context.i = context;
}

