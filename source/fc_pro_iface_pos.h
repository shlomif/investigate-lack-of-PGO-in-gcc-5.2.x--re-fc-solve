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
 * fc_pro_iface_pos.h - generate solutions in standard notation, with
 * implicit (and not included) Horne/Raymond prune moves.
 */
#ifndef FC_SOLVE__FC_PRO_IFACE_POS_H
#define FC_SOLVE__FC_PRO_IFACE_POS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "fcs_user.h"
#include "fcs_cl.h"

#include "bool.h"
#include "inline.h"

#include "state.h"

typedef struct
{
    fcs_move_t move;
    fcs_bool_t to_empty_stack;
} fcs_extended_move_t;

typedef struct
{
    int next_move_idx;
    int num_moves;
    fcs_extended_move_t * moves;
} fcs_moves_processed_t;

extern void fc_solve_moves_processed_gen(
    fcs_moves_processed_t * const ret,
    fcs_state_keyval_pair_t * const orig,
    const int num_freecells,
    const fcs_moves_sequence_t * const moves_seq
);

char * fc_solve_moves_processed_render_move(fcs_extended_move_t move, char * string);

static GCC_INLINE const int fc_solve_moves_processed_get_moves_left(const fcs_moves_processed_t * const moves)
{
    return moves->num_moves - moves->next_move_idx;
}

static GCC_INLINE fcs_bool_t fc_solve_moves_processed_get_next_move(fcs_moves_processed_t * const moves, fcs_extended_move_t * const move)
{
    if (moves->next_move_idx == moves->num_moves)
    {
        return TRUE;
    }
    *move = moves->moves[moves->next_move_idx++];
    return FALSE;
}

static GCC_INLINE void fc_solve_moves_processed_free(fcs_moves_processed_t * moves)
{
    free(moves->moves);
    moves->moves = NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* #ifndef FC_SOLVE__FC_PRO_IFACE_POS_H */
