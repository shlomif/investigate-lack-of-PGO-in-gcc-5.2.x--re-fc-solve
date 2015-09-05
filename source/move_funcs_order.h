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
 * move_funcs_order.h - contains the fc_solve_apply_tests_order function.
 *
 */

#ifndef FC_SOLVE__MOVE_FUNCS_ORDER_H
#define FC_SOLVE__MOVE_FUNCS_ORDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <string.h>

#include "instance.h"

#include "inline.h"

static GCC_INLINE int fc_solve_string_to_test_num_compare_func(
        const void * const a,
        const void * const b
        )
{
    return
        strcmp(
            ((const fcs_move_func_aliases_mapping_t *)a)->alias,
            ((const fcs_move_func_aliases_mapping_t *)b)->alias
        );
}

static GCC_INLINE const int fc_solve_string_to_test_num(const char * const s)
{
    fcs_move_func_aliases_mapping_t needle;
    needle.alias[0] = s[0];
    needle.alias[1] = '\0';

    const fcs_move_func_aliases_mapping_t * const result = (fcs_move_func_aliases_mapping_t *)
        bsearch(
            &needle,
            fc_solve_sfs_move_funcs_aliases,
            FCS_MOVE_FUNCS_ALIASES_NUM,
            sizeof(fc_solve_sfs_move_funcs_aliases[0]),
            fc_solve_string_to_test_num_compare_func
            );

    return (result ? result->move_func_num : 0);
}

extern int fc_solve_apply_tests_order(
    fcs_tests_order_t * tests_order,
    const char * string,
    char * * error_string
    );

#ifdef __cplusplus
}
#endif

#endif /* FC_SOLVE__MOVE_FUNCS_ORDER_H */
