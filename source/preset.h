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
 * preset.h - header file of the preset management functions for Freecell Solver.
 *
 * This header is internal to Freecell Solver.
 */

#ifndef FC_SOLVE__PRESET_H
#define FC_SOLVE__PRESET_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

#ifndef FCS_FREECELL_ONLY
#include "instance.h"
#include "move_funcs_maps.h"

typedef struct
{
    int preset_id;

    fcs_game_type_params_t game_params;

    char tests_order[FCS_MOVE_FUNCS_NUM*3+1];
    char allowed_tests[FCS_MOVE_FUNCS_NUM*3+1];
} fcs_preset_t;

extern const fc_solve_preset_ret_code_t fc_solve_apply_preset_by_ptr(
    fc_solve_instance_t * const instance,
    const fcs_preset_t * const preset_ptr
    );

extern const fc_solve_preset_ret_code_t fc_solve_get_preset_by_name(
    const char * const name,
    const fcs_preset_t * * const preset_ptr
    );

static GCC_INLINE const fc_solve_preset_ret_code_t fc_solve_apply_preset_by_name(
    fc_solve_instance_t * const instance,
    const char * const name
    )
{
    const fcs_preset_t * preset_ptr;

    const int ret = fc_solve_get_preset_by_name(
        name,
        &preset_ptr
        );

    if (ret != FCS_PRESET_CODE_OK)
    {
        return ret;
    }

    return fc_solve_apply_preset_by_ptr(instance, preset_ptr);
}


#define fcs_duplicate_preset(d,s) ((d) = (s))

#endif /* FCS_FREECELL_ONLY */

#ifdef __cplusplus
}
#endif

#endif
