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
 * delta_states_iface.h - some public interface functions for delta_state.c
 * (without all the baggage).
 *
 */

#ifndef FC_SOLVE__DELTA_STATES_IFACE_H
#define FC_SOLVE__DELTA_STATES_IFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "dbm_common.h"

extern char * fc_solve_user_INTERNAL_delta_states_enc_and_dec(
        enum fcs_dbm_variant_type_t local_variant,
        const char * init_state_str_proto,
        const char * derived_state_str_proto
        );

#ifdef __cplusplus
}
#endif

#endif  /* FC_SOLVE__DELTA_STATES_IFACE_H */
