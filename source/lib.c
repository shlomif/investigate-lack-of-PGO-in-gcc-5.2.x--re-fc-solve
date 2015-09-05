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
 * lib.c - library interface functions of Freecell Solver.
 *
 */
#define BUILDING_DLL 1

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#include "state.h"
#include "instance_for_lib.h"
#include "preset.h"
#include "fcs_user.h"
#include "move_funcs_order.h"
#include "fcs_user_internal.h"
#include "fc_pro_iface_pos.h"

#include "unused.h"
#include "bool.h"
#include "indirect_buffer.h"
#include "count.h"
#include "min_and_max.h"
#include "alloc_wrap.h"

#include "str_utils.h"

#define FCS_MAX_FLARE_NAME_LEN 30

typedef struct {
    fcs_int_limit_t num_checked_states;
    fcs_int_limit_t num_states_in_collection;
} fcs_stats_t;

static GCC_INLINE const fcs_stats_t calc_initial_stats_t(void)
{
    return (const fcs_stats_t) {.num_checked_states = 0, .num_states_in_collection = 0};
}

/* A flare is an alternative scan algorithm to be tried. All flares in
 * a single instance are being evaluated and then one picks the shortest
 * solution out of all of them. (see fc-solve/docs/flares-functional-spec.txt )
 * */

typedef struct
{
    fc_solve_instance_t obj;
    int ret_code;
    /* Whether the instance is ready to be input with (i.e:
     * was recycled already.) */
    fcs_bool_t instance_is_ready;
    int limit;
    char name[FCS_MAX_FLARE_NAME_LEN];
    int next_move;
    fcs_moves_sequence_t moves_seq;
    fcs_moves_processed_t fc_pro_moves;
    fcs_stats_t obj_stats;
    fcs_bool_t was_solution_traced;
    fcs_state_locs_struct_t trace_solution_state_locs;
} fcs_flare_item_t;

typedef enum
{
    FLARES_PLAN_RUN_INDEFINITELY,
    FLARES_PLAN_RUN_COUNT_ITERS,
    FLARES_PLAN_CHECKPOINT,
} flares_plan_type_t;

typedef enum
{
    FLARES_CHOICE_FC_SOLVE_SOLUTION_LEN,
    FLARES_CHOICE_FCPRO_SOLUTION_LEN
} flares_choice_type_t;

typedef struct
{
    flares_plan_type_t type;
    int flare_idx;
    int count_iters;
} flares_plan_item;

typedef struct
{
    int num_flares;
    fcs_flare_item_t * flares;
    flares_plan_item * plan;
    int num_plan_items;
    int current_plan_item_idx;
    int minimal_solution_flare_idx;
    int all_plan_items_finished_so_far;
    char * flares_plan_string;
    /*
     * The default flares_plan_compiled is "False", which means that the
     * flares_plan_string was set and needs to be processed. Once
     * the compile function is called, it is set to "True" and it is set
     * to "False" if the flares_plan_string is set to a different value.
     *
     * Upon starting to run, one checks if flares_plan_compiled is false
     * and if so, compiles the flares plan, and sets the flares_plan_compiled
     * string to true.
     */
    fcs_bool_t flares_plan_compiled;
    int limit;
}  fcs_instance_item_t;

typedef struct
{
    /*
     * This is a list of several consecutive instances that are run
     * one after the other in case the previous ones could not solve
     * the board
     * */
    fcs_instance_item_t * instances_list;
    int num_instances;

    int current_instance_idx;
    /*
     * The global (sequence-wide) limit of the iterations. Used
     * by limit_iterations() and friends
     * */
    fcs_int_limit_t current_iterations_limit;
    /*
     * The number of iterations this board started at.
     * */
    fcs_stats_t iterations_board_started_at;
    /*
     * The number of iterations that the current instance started solving from.
     * */
    fcs_stats_t init_num_checked_states;
    /*
     * A pointer to the currently active instance out of the sequence
     * */
    fcs_flare_item_t * active_flare;
    fcs_state_keyval_pair_t state;
    fcs_state_keyval_pair_t running_state;
    fcs_state_keyval_pair_t initial_non_canonized_state;
    fcs_state_locs_struct_t state_locs;
    fcs_state_locs_struct_t trace_solution_state_locs;
    fcs_state_locs_struct_t initial_state_locs;
    int ret_code;
    fcs_bool_t all_instances_were_suspended;
    fcs_state_validity_ret_t state_validity_ret;
    fcs_card_t state_validity_card;
    freecell_solver_user_iter_handler_t iter_handler;
    freecell_solver_user_long_iter_handler_t long_iter_handler;
    void * iter_handler_context;
    flares_choice_type_t flares_choice;
    double flares_iters_factor;

    fc_solve_soft_thread_t * soft_thread;

    DECLARE_IND_BUF_T(indirect_stacks_buffer)
    char * state_string_copy;

#ifndef FCS_FREECELL_ONLY
    fcs_preset_t common_preset;
#endif

    char * error_string;

    fcs_meta_compact_allocator_t meta_alloc;
} fcs_user_t;


static void iter_handler_wrapper(
    void * api_instance,
    fcs_int_limit_t iter_num,
    int depth,
    void * lp_instance GCC_UNUSED,
    fcs_kv_state_t * ptr_state,
    fcs_int_limit_t parent_iter_num
    );

#define FLARES_LOOP_START() \
    for(int user_inst_idx = 0 ; user_inst_idx < user->num_instances ; user_inst_idx++) \
    { \
        fcs_instance_item_t * const instance_item = &(user->instances_list[user_inst_idx]); \
                  \
        for(int flare_idx = 0; flare_idx < instance_item->num_flares; flare_idx++) \
        {      \
            fcs_flare_item_t * const flare = &(instance_item->flares[flare_idx]); \


#define FLARES_LOOP_END_FLARES() \
        }

#define FLARES_LOOP_END_INSTANCES() \
    }

#define FLARES_LOOP_END() \
    FLARES_LOOP_END_FLARES() \
    FLARES_LOOP_END_INSTANCES()

static int user_next_instance(fcs_user_t * user);

static void user_initialize(
        fcs_user_t * const user
        )
{
#ifndef FCS_FREECELL_ONLY
    const fcs_preset_t * freecell_preset;

    fc_solve_get_preset_by_name(
        "freecell",
        &freecell_preset
        );

    fcs_duplicate_preset(user->common_preset, *freecell_preset);
#endif

    fc_solve_meta_compact_allocator_init(&(user->meta_alloc));

    user->instances_list = NULL;
    user->num_instances = 0;
    user->long_iter_handler = NULL;
    user->iter_handler = NULL;
    user->current_iterations_limit = -1;

    user->state_string_copy = NULL;
    user->iterations_board_started_at = calc_initial_stats_t();
    user->all_instances_were_suspended = TRUE;
    user->flares_choice = FLARES_CHOICE_FC_SOLVE_SOLUTION_LEN;
    user->flares_iters_factor = 1.0;

    user->error_string = NULL;

    user_next_instance(user);

    return;
}

void DLLEXPORT * freecell_solver_user_alloc(void)
{
    fcs_user_t * const ret = (fcs_user_t *)SMALLOC1(ret);

    user_initialize(ret);

    return (void*)ret;
}

int DLLEXPORT freecell_solver_user_apply_preset(
    void * api_instance,
    const char * preset_name)
{
#ifdef FCS_FREECELL_ONLY
    return FCS_PRESET_CODE_OK;
#else
    const fcs_preset_t * new_preset_ptr;
    int status;
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    status =
        fc_solve_get_preset_by_name(
            preset_name,
            &new_preset_ptr
            );

    if (status != FCS_PRESET_CODE_OK)
    {
        return status;
    }

    FLARES_LOOP_START()
        status = fc_solve_apply_preset_by_ptr(
            &(flare->obj),
            new_preset_ptr
            );

        if (status != FCS_PRESET_CODE_OK)
        {
            return status;
        }
    FLARES_LOOP_END()

    fcs_duplicate_preset(user->common_preset, *new_preset_ptr);

    return FCS_PRESET_CODE_OK;
#endif
}

void DLLEXPORT freecell_solver_user_limit_iterations_long(
    void * const api_instance,
    const fcs_int_limit_t max_iters
    )
{
    ((fcs_user_t * const)api_instance)->current_iterations_limit = max_iters;
}

void DLLEXPORT freecell_solver_user_limit_iterations(
    void * const api_instance,
    const int max_iters
    )
{
    return freecell_solver_user_limit_iterations_long(api_instance, (fcs_int_limit_t)max_iters);
}

void DLLEXPORT freecell_solver_user_limit_current_instance_iterations(
    void * const api_instance,
    const int max_iters
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    user->instances_list[user->current_instance_idx].limit = max_iters;
}

int DLLEXPORT freecell_solver_user_set_depth_tests_order(
    void * api_instance,
    int min_depth,
    const char * tests_order,
    char * * error_string
    )
{
    int depth_idx;
    int ret_code;

    fcs_user_t * const user = (fcs_user_t *)api_instance;

    *error_string = NULL;

    if (min_depth < 0)
    {
        *error_string = strdup("Depth is negative.");
        return 1;
    }

    if (min_depth == 0)
    {
        depth_idx = 0;
    }
    else
    {
        for (depth_idx = 0
                ;
                ;
                depth_idx++
            )
        {
            if (depth_idx == user->soft_thread->by_depth_tests_order.num-1)
            {
                break;
            }
            else if (min_depth < user->soft_thread
                    ->by_depth_tests_order.by_depth_tests[depth_idx].max_depth
                    )
            {
                break;
            }
        }

        depth_idx++;
    }

    if (depth_idx == user->soft_thread->by_depth_tests_order.num)
    {
        user->soft_thread->by_depth_tests_order.by_depth_tests =
            SREALLOC(
                user->soft_thread->by_depth_tests_order.by_depth_tests,
                ++user->soft_thread->by_depth_tests_order.num
                );

        user->soft_thread->by_depth_tests_order.by_depth_tests[depth_idx].tests_order.num_groups = 0;
        user->soft_thread->by_depth_tests_order.by_depth_tests[depth_idx].tests_order.groups = NULL;
    }

    if (depth_idx > 0)
    {
        user->soft_thread->
            by_depth_tests_order.by_depth_tests[depth_idx-1].max_depth = min_depth;
    }

    user->soft_thread->
        by_depth_tests_order.by_depth_tests[depth_idx].max_depth = INT_MAX;

    ret_code =
        fc_solve_apply_tests_order(
            &(user->soft_thread->by_depth_tests_order.by_depth_tests[depth_idx].tests_order),
            tests_order,
            error_string
            );

    {
        int further_depth_idx;
        for (further_depth_idx = depth_idx+1; further_depth_idx < user->soft_thread->by_depth_tests_order.num ; further_depth_idx++)
        {
            fc_solve_free_tests_order(&(
                user->soft_thread->by_depth_tests_order
                .by_depth_tests[further_depth_idx].tests_order
            ));
        }
    }

    user->soft_thread->by_depth_tests_order.by_depth_tests =
        SREALLOC(
            user->soft_thread->by_depth_tests_order.by_depth_tests,
            user->soft_thread->by_depth_tests_order.num = depth_idx+1
        );

    return ret_code;
}

int DLLEXPORT freecell_solver_user_set_tests_order(
    void * api_instance,
    const char * tests_order,
    char * * error_string
    )
{
    return freecell_solver_user_set_depth_tests_order(
            api_instance, 0, tests_order, error_string
    );
}

typedef enum
{
    FCS_COMPILE_FLARES_RET_OK = 0,
    FCS_COMPILE_FLARES_RET_COLON_NOT_FOUND,
    FCS_COMPILE_FLARES_RET_RUN_AT_SIGN_NOT_FOUND,
    FCS_COMPILE_FLARES_RET_UNKNOWN_FLARE_NAME,
    FCS_COMPILE_FLARES_RET_JUNK_AFTER_CP,
    FCS_COMPILE_FLARES_RET_UNKNOWN_COMMAND,
    FCS_COMPILE_FLARES_RUN_JUNK_AFTER_LAST_RUN_INDEF
} fcs_compile_flares_ret_t;

static GCC_INLINE const flares_plan_item create_plan_item(
    const flares_plan_type_t mytype,
    const int flare_idx,
    const int count_iters
)
{
    return (const flares_plan_item) {.type = mytype, .flare_idx = flare_idx, .count_iters = count_iters};
}

static GCC_INLINE const flares_plan_type_t add_to_plan(
        fcs_instance_item_t * const instance_item,
        const flares_plan_type_t mytype,
        const int flare_idx,
        const int count_iters
    )
{
    const int next_item = instance_item->num_plan_items;

    instance_item->plan =
        SREALLOC( instance_item->plan, ++(instance_item->num_plan_items));

    instance_item->plan[next_item] = create_plan_item(mytype, flare_idx, count_iters);

    return mytype;
}

static GCC_INLINE const flares_plan_type_t add_count_iters_to_plan(
        fcs_instance_item_t * const instance_item,
        const int flare_idx,
        const int count_iters
)
{
    return add_to_plan(instance_item,
            FLARES_PLAN_RUN_COUNT_ITERS, flare_idx, count_iters
            );
}


static GCC_INLINE const flares_plan_type_t add_checkpoint_to_plan(
        fcs_instance_item_t * const instance_item
    )
{
    return add_to_plan(instance_item,
            FLARES_PLAN_CHECKPOINT, -1, -1
            );
}

static GCC_INLINE const flares_plan_type_t add_run_indef_to_plan(
        fcs_instance_item_t * const instance_item,
        const int flare_idx
    )
{
    return add_to_plan(instance_item,
            FLARES_PLAN_RUN_INDEFINITELY, flare_idx, -1
            );
}

static GCC_INLINE fcs_compile_flares_ret_t user_compile_all_flares_plans(
    fcs_user_t * const user,
    int * const instance_list_index,
    char * * const error_string
    )
{
    for (int user_inst_idx = 0 ; user_inst_idx < user->num_instances ; user_inst_idx++)
    {
        *instance_list_index = user_inst_idx;

        fcs_instance_item_t * const instance_item = &(user->instances_list[user_inst_idx]);

        if (instance_item->flares_plan_compiled)
        {
            continue;
        }

        /* If the plan string is NULL or empty, then set the plan
         * to run only the first flare indefinitely. (And then have
         * an implicit checkpoint for good measure.) */
        if ((! instance_item->flares_plan_string) ||
           (! instance_item->flares_plan_string[0]))
        {
            if (instance_item->plan)
            {
                free(instance_item->plan);
            }
            instance_item->num_plan_items = 2;
            instance_item->plan = SMALLOC(
                instance_item->plan,
                instance_item->num_plan_items
            );
            /* Set to the first flare. */
            instance_item->plan[0] = create_plan_item(FLARES_PLAN_RUN_INDEFINITELY, 0, -1);
            instance_item->plan[1] = create_plan_item(FLARES_PLAN_CHECKPOINT, -1, -1);

            instance_item->flares_plan_compiled = TRUE;
            continue;
        }

        /* Tough luck - gotta parse the string. ;-) */
        {
            char * item_start, * item_end, * cmd_end;
            flares_plan_type_t last_item_type;

            if (instance_item->plan)
            {
                free(instance_item->plan);
                instance_item->plan = NULL;
                instance_item->num_plan_items = 0;
            }

            item_start = instance_item->flares_plan_string;

            do
            {
                cmd_end = strchr(item_start, ':');

                if (! cmd_end)
                {
                    *error_string = strdup("Could not find a \":\" for a command.");
                    *instance_list_index = user_inst_idx;
                    return FCS_COMPILE_FLARES_RET_COLON_NOT_FOUND;
                }

                if (string_starts_with(item_start, "Run", cmd_end))
                {
                    char * at_sign, * after_at_sign;
                    int count_iters;
                    fcs_bool_t found_flare;
                    int flare_idx;

                    /* It's a Run item - handle it. */
                    cmd_end++;
                    count_iters = atoi(cmd_end);

                    at_sign = cmd_end;

                    while ((*at_sign) && isdigit(*at_sign))
                    {
                        at_sign++;
                    }

                    if (*at_sign != '@')
                    {
                        *error_string = strdup("Could not find a \"@\" directly after the digits after the 'Run:' command.");
                        *instance_list_index = user_inst_idx;
                        return FCS_COMPILE_FLARES_RET_RUN_AT_SIGN_NOT_FOUND;
                    }
                    after_at_sign = at_sign+1;

                    /*
                     * Position item_end at the end of item (designated by ",")
                     * or alternatively the end of the string.
                     * */
                    item_end = strchr(after_at_sign, ',');
                    if (!item_end)
                    {
                        item_end = after_at_sign+strlen(after_at_sign);
                    }

                    found_flare = 0;
                    for(flare_idx = 0; flare_idx < instance_item->num_flares; flare_idx++)
                    {
                        fcs_flare_item_t * const flare = &(instance_item->flares[flare_idx]);

                        if (!strncmp(flare->name, after_at_sign, item_end-after_at_sign))
                        {
                            found_flare = 1;
                            break;
                        }
                    }

                    if (! found_flare)
                    {
                        /* TODO : write what the flare name is.  */
                        *error_string = strdup("Unknown flare name.");
                        *instance_list_index = user_inst_idx;
                        return FCS_COMPILE_FLARES_RET_UNKNOWN_FLARE_NAME;
                    }

                    /* TODO : free plan upon an error. */
                    last_item_type = add_count_iters_to_plan(instance_item, flare_idx, count_iters);
                }
                else if (string_starts_with(item_start, "CP", cmd_end))
                {
                    item_end = cmd_end+1;
                    if (! (((*item_end) == ',') || (! (*item_end))))
                    {
                        *error_string = strdup("Junk after CP (Checkpoint) command.");
                        *instance_list_index = user_inst_idx;
                        return FCS_COMPILE_FLARES_RET_JUNK_AFTER_CP;
                    }

                    /* TODO : free plan upon an error. */
                    last_item_type = add_checkpoint_to_plan(instance_item);
                }
                else if (string_starts_with(item_start, "RunIndef", cmd_end))
                {
                    fcs_bool_t found_flare;
                    int flare_idx;

                    cmd_end++;
                    item_end = strchr(cmd_end, ',');
                    if (item_end)
                    {
                        *error_string = strdup("Junk after last RunIndef command. Must be the final command.");
                        *instance_list_index = user_inst_idx;
                        return FCS_COMPILE_FLARES_RUN_JUNK_AFTER_LAST_RUN_INDEF;
                    }
                    item_end = cmd_end+strlen(cmd_end);

                    found_flare = FALSE;
                    for(flare_idx = 0; flare_idx < instance_item->num_flares; flare_idx++)
                    {
                        fcs_flare_item_t * const flare = &(instance_item->flares[flare_idx]);

                        if (!strncmp(flare->name, cmd_end, item_end-cmd_end))
                        {
                            found_flare = TRUE;
                            break;
                        }
                    }

                    if (! found_flare)
                    {
                        /* TODO : write what the flare name is.  */
                        *error_string = strdup("Unknown flare name in RunIndef command.");
                        *instance_list_index = user_inst_idx;
                        return FCS_COMPILE_FLARES_RET_UNKNOWN_FLARE_NAME;
                    }

                    last_item_type = add_run_indef_to_plan(instance_item, flare_idx);
                }
                else
                {
                    /* TODO : Write the unknown command in the error string. */
                    *error_string = strdup("Unknown command.");
                    *instance_list_index = user_inst_idx;
                    return FCS_COMPILE_FLARES_RET_UNKNOWN_COMMAND;
                }
                item_start = item_end+1;
            } while (*item_end);

            if (last_item_type != FLARES_PLAN_CHECKPOINT)
            {
                add_checkpoint_to_plan(instance_item);
            }

            instance_item->flares_plan_compiled = TRUE;
            continue;
        }
    }

    *instance_list_index = -1;
    *error_string = NULL;

    return FCS_COMPILE_FLARES_RET_OK;
}

/*
 * Add a trailing newline to the string if it does not exist.
 */
#define MY_MARGIN 2
#define TRAILING_CHAR '\n'
static GCC_INLINE char * duplicate_string_while_adding_a_trailing_newline(
    const char * const orig_str
)
{
    const int len = strlen(orig_str);
    char * s = malloc(len + MY_MARGIN);
    strcpy (s, orig_str);
    {
        char * s_end = s + len - 1;
        if ((*s_end) != TRAILING_CHAR)
        {
            *(++s_end) = TRAILING_CHAR;
            *(++s_end) = '\0';
        }
    }
    return s;
}
#undef TRAILING_CHAR
#undef MY_MARGIN

int DLLEXPORT freecell_solver_user_solve_board(
    void * const api_instance,
    const char * const state_as_string
    )
{
    char * error_string;

    fcs_user_t * const user = (fcs_user_t *)api_instance;

    user->state_string_copy =
        duplicate_string_while_adding_a_trailing_newline(state_as_string);

    user->current_instance_idx = 0;

    int instance_list_index;
    if (
        user_compile_all_flares_plans(
            user,
            &instance_list_index,
            &error_string
        ) != FCS_COMPILE_FLARES_RET_OK
    )
    {
        user->error_string = error_string;

        return FCS_STATE_FLARES_PLAN_ERROR;
    }

    return freecell_solver_user_resume_solution(api_instance);
}

static GCC_INLINE void recycle_flare(
    fcs_flare_item_t * const flare
)
{
    if (! flare->instance_is_ready)
    {
        fc_solve_recycle_instance(&(flare->obj));
        flare->instance_is_ready = TRUE;
    }
}

static void recycle_instance(
    fcs_user_t * const user,
    const int i
    )
{
    fcs_instance_item_t * const instance_item = &(user->instances_list[i]);

    for(int flare_idx = 0; flare_idx < instance_item->num_flares ; flare_idx++)
    {
        fcs_flare_item_t * flare;

        flare = &(instance_item->flares[flare_idx]);

        if (flare->fc_pro_moves.moves)
        {
            fc_solve_moves_processed_free(&(flare->fc_pro_moves));
        }

        if (flare->ret_code != FCS_STATE_NOT_BEGAN_YET)
        {
            recycle_flare( flare );
            /*
             * We have to initialize init_num_checked_states to 0 here, because it may
             * not get initialized again, and now the num_checked_states of the instance
             * is equal to 0.
             * */
            user->init_num_checked_states = calc_initial_stats_t();

            flare->ret_code = FCS_STATE_NOT_BEGAN_YET;
        }

        if (flare->moves_seq.moves)
        {
            free (flare->moves_seq.moves);
            flare->moves_seq.moves = NULL;
            flare->moves_seq.num_moves = 0;
            flare->next_move = 0;
        }

        flare->obj_stats = calc_initial_stats_t();
    }

    instance_item->current_plan_item_idx = 0;
    instance_item->minimal_solution_flare_idx = -1;

    return;
}

#ifndef FCS_USE_COMPACT_MOVE_TOKENS
#define internal_move_to_user_move(x) (x)
#else
static GCC_INLINE const fcs_move_t internal_move_to_user_move(const fcs_internal_move_t internal_move)
{
    fcs_move_t user_move;

    /* Convert the internal_move to a user move. */
    fcs_move_set_src_stack(user_move, fcs_int_move_get_src_stack(internal_move));
    fcs_move_set_dest_stack(user_move, fcs_int_move_get_dest_stack(internal_move));
    fcs_move_set_type(user_move, fcs_int_move_get_type(internal_move));
    fcs_move_set_num_cards_in_seq(user_move, fcs_int_move_get_num_cards_in_seq(internal_move));

    return user_move;
}
#endif

#ifndef FCS_USE_COMPACT_MOVE_TOKENS
#define user_move_to_internal_move(x) (x)
#else
static GCC_INLINE const fcs_internal_move_t user_move_to_internal_move(const fcs_move_t user_move)
{
    fcs_internal_move_t internal_move;

    /* Convert the internal_move to a user move. */
    fcs_int_move_set_src_stack(internal_move, fcs_move_get_src_stack(user_move));
    fcs_int_move_set_dest_stack(internal_move, fcs_move_get_dest_stack(user_move));
    fcs_int_move_set_type(internal_move, fcs_move_get_type(user_move));
    fcs_int_move_set_num_cards_in_seq(internal_move, fcs_move_get_num_cards_in_seq(user_move));

    return internal_move;
}
#endif

static GCC_INLINE int calc_moves_seq(
    const fcs_move_stack_t * const solution_moves,
    fcs_moves_sequence_t * const moves_seq
)
{
    moves_seq->num_moves = 0;
    moves_seq->moves = NULL;

    int num_moves = (int)solution_moves->num_moves;
    fcs_internal_move_t * next_move_ptr = solution_moves->moves + num_moves - 1;

    fcs_move_t * ret_moves = SMALLOC(ret_moves, num_moves);

    if (!ret_moves)
    {
        return -1;
    }

    for (int i = 0 ; i < num_moves ; i++)
    {
        ret_moves[i] = internal_move_to_user_move(*(next_move_ptr--));
    }

    moves_seq->num_moves = num_moves;
    moves_seq->moves = ret_moves;

    return 0;
}


static void trace_flare_solution(
    fcs_user_t * const user,
    fcs_flare_item_t * const flare
)
{
    if (flare->was_solution_traced)
    {
        return;
    }

    fc_solve_instance_t * const instance =
        &(flare->obj);

    fc_solve_trace_solution(instance);
    fcs_kv_state_t pass = FCS_STATE_keyval_pair_to_kv(&(user->state));
    flare->trace_solution_state_locs = user->state_locs;
    /*
     * TODO : maybe only normalize the final moves' stack in
     * order to speed things up.
     * */
    fc_solve_move_stack_normalize(
        &(instance->solution_moves),
        &(pass),
        &(flare->trace_solution_state_locs),
        INSTANCE_FREECELLS_NUM,
        INSTANCE_STACKS_NUM,
        INSTANCE_DECKS_NUM
    );

    calc_moves_seq(
        &(instance->solution_moves),
        &(flare->moves_seq)
    );
    flare->next_move = 0;
    if (instance->solution_moves.moves)
    {
        fcs_move_stack_static_destroy(instance->solution_moves);
        instance->solution_moves.moves = NULL;
    }

    flare->obj_stats.num_checked_states = flare->obj.i__num_checked_states;
    flare->obj_stats.num_states_in_collection = flare->obj.num_states_in_collection;

    recycle_flare( flare );
    flare->was_solution_traced = TRUE;
}

static int get_flare_move_count(
    fcs_user_t * const user,
    fcs_flare_item_t * const flare
)
{
    trace_flare_solution(user, flare);
    if (user->flares_choice == FLARES_CHOICE_FC_SOLVE_SOLUTION_LEN)
    {
        return flare->moves_seq.num_moves;
    }
    else
    {
        if (! flare->fc_pro_moves.moves)
        {
            fc_solve_moves_processed_gen(
                &(flare->fc_pro_moves),
                &(user->initial_non_canonized_state),
#ifdef FCS_FREECELL_ONLY
                4,
#else
                user->common_preset.game_params.freecells_num,
#endif
                &(flare->moves_seq)
            );
        }

        return fc_solve_moves_processed_get_moves_left(&(flare->fc_pro_moves));
    }
}

static GCC_INLINE fcs_instance_item_t * get_current_instance_item(
    fcs_user_t const * user
)
{
    return &(user->instances_list[user->current_instance_idx]);
}

typedef int flare_iters_quota_t;

static GCC_INLINE const flare_iters_quota_t normalize_iters_quota(
    const flare_iters_quota_t i
)
{
    return max(i, 0);
}

int DLLEXPORT freecell_solver_user_resume_solution(
    void * const api_instance
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    fcs_stats_t init_num_checked_states;

    int ret = FCS_STATE_IS_NOT_SOLVEABLE;

    /*
     * I expect user->current_instance_idx to be initialized with some value.
     * */
    do
    {
        fcs_instance_item_t * const instance_item =
            get_current_instance_item(user);

        if (instance_item->current_plan_item_idx ==
                instance_item->num_plan_items
           )
        {
            /*
             * If all the plan items finished so far, it means this instance
             * cannot be reused, because it will always yield a cannot
             * be found result. So instead of looping infinitely,
             * move to the next instance, or exit. */
            if (instance_item->all_plan_items_finished_so_far)
            {
                recycle_instance(user, user->current_instance_idx);
                user->current_instance_idx++;
                continue;
            }
            /* Otherwise - restart the plan again. */
            else
            {
                instance_item->all_plan_items_finished_so_far = 1;
                instance_item->current_plan_item_idx = 0;
            }
        }

        const flares_plan_item * const current_plan_item =
            &(instance_item->plan[instance_item->current_plan_item_idx++]);


        if (current_plan_item->type == FLARES_PLAN_CHECKPOINT)
        {
            if (instance_item->minimal_solution_flare_idx >= 0)
            {
                user->active_flare = &(instance_item->flares[instance_item->minimal_solution_flare_idx]);
                user->init_num_checked_states = user->active_flare->obj_stats;

                ret = user->ret_code = FCS_STATE_WAS_SOLVED;

                break;
            }
            else
            {
                continue;
            }
        }

        const flare_iters_quota_t flare_iters_quota =
        (
            (current_plan_item->type == FLARES_PLAN_RUN_INDEFINITELY)
            ? -1
            /* (current_plan_item->type == FLARES_PLAN_RUN_COUNT_ITERS)  */
            : normalize_iters_quota(
                (typeof(flare_iters_quota))
                (user->flares_iters_factor * current_plan_item->count_iters)
            )
        );

        const int flare_idx = current_plan_item->flare_idx;
        fcs_flare_item_t * const flare =
            &(instance_item->flares[flare_idx]);

        /* TODO : For later - loop over the flares based on the flares plan. */
        user->active_flare = flare;

        const fcs_bool_t is_start_of_flare_solving =
            (flare->ret_code == FCS_STATE_NOT_BEGAN_YET);

        if (is_start_of_flare_solving)
        {
#if (!(defined(HARD_CODED_NUM_FREECELLS) && defined(HARD_CODED_NUM_STACKS) && defined(HARD_CODED_NUM_DECKS)))
            fc_solve_instance_t * const instance = &(user->active_flare->obj);
#endif

            if (!
                fc_solve_initial_user_state_to_c(
                    user->state_string_copy,
                    &(user->state),
                    INSTANCE_FREECELLS_NUM,
                    INSTANCE_STACKS_NUM,
                    INSTANCE_DECKS_NUM,
                    user->indirect_stacks_buffer
                )
            )
            {
                user->state_validity_ret = FCS_STATE_VALIDITY__PREMATURE_END_OF_INPUT;
                return (user->ret_code = FCS_STATE_INVALID_STATE);
            }

            if (
                FCS_STATE_VALIDITY__OK
                    !=
                (
                    user->state_validity_ret = fc_solve_check_state_validity(
                        &(user->state),
                        INSTANCE_FREECELLS_NUM,
                        INSTANCE_STACKS_NUM,
                        INSTANCE_DECKS_NUM,
                        &(user->state_validity_card)
                    )
                )
            )
            {
                return (user->ret_code = FCS_STATE_INVALID_STATE);
            }

            fc_solve_init_locs(&(user->initial_state_locs));
            user->state_locs = user->initial_state_locs;

            fcs_kv_state_t state_pass = FCS_STATE_keyval_pair_to_kv(&(user->state));
            /* running_state and initial_non_canonized_state are
             * normalized states. So We're duplicating
             * state to it before state state_pass is canonized.
             * */
            {
                fcs_kv_state_t pass = FCS_STATE_keyval_pair_to_kv(&(user->running_state));

                fcs_duplicate_kv_state(&pass, &state_pass);
            }

            {
                fcs_kv_state_t initial_pass = FCS_STATE_keyval_pair_to_kv(&(user->initial_non_canonized_state));

                fcs_duplicate_kv_state(&initial_pass, &state_pass);
            }

            fc_solve_canonize_state_with_locs
                (
                 &state_pass,
                &(user->state_locs),
                INSTANCE_FREECELLS_NUM,
                INSTANCE_STACKS_NUM
                );

            user->trace_solution_state_locs = user->state_locs;

            fc_solve_init_instance(&(user->active_flare->obj));
        }

#define PARAMETERIZED_FIXED_LIMIT(increment) \
    (user->iterations_board_started_at.num_checked_states + increment)
#define PARAMETERIZED_LIMIT(increment) (((increment) < 0) ? (-1) : PARAMETERIZED_FIXED_LIMIT(increment))
#define local_limit()  \
        (instance_item->limit)
#define NUM_ITERS_LIMITS 3
#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif

        {
            fcs_int_limit_t limits[NUM_ITERS_LIMITS];
            int limit_idx;
            fcs_int_limit_t mymin, new_lim;

            limits[0] = local_limit();
            limits[1] = user->current_iterations_limit;
            limits[2] = PARAMETERIZED_LIMIT(flare_iters_quota);

            mymin = limits[0];
            for (limit_idx=1;limit_idx<NUM_ITERS_LIMITS;limit_idx++)
            {
                new_lim = limits[limit_idx];
                if (new_lim >= 0)
                {
                    mymin = (mymin < 0) ? new_lim : min(mymin, new_lim);
                }
            }

            if (mymin < 0)
            {
                user->active_flare->obj.i__max_num_checked_states = -1;
                user->active_flare->obj.effective_max_num_checked_states = FCS_INT_LIMIT_MAX;
            }
            else
            {
                user->active_flare->obj.i__max_num_checked_states =
                    user->active_flare->obj.effective_max_num_checked_states =
                    (user->active_flare->obj.i__num_checked_states + mymin - user->iterations_board_started_at.num_checked_states);
            }
        }

        user->init_num_checked_states.num_checked_states = init_num_checked_states.num_checked_states = user->active_flare->obj.i__num_checked_states;
        user->init_num_checked_states.num_states_in_collection = init_num_checked_states.num_states_in_collection = user->active_flare->obj.num_states_in_collection;

        if (is_start_of_flare_solving)
        {
            fc_solve_start_instance_process_with_board(
                &(user->active_flare->obj), &(user->state),
                &(user->initial_non_canonized_state)
            );
        }

        const fcs_bool_t was_run_now = (
            (flare->ret_code == FCS_STATE_SUSPEND_PROCESS)
            || (flare->ret_code == FCS_STATE_NOT_BEGAN_YET)
        );

        if (was_run_now)
        {
            ret = user->ret_code =
                flare->ret_code =
                    fc_solve_resume_instance(&(user->active_flare->obj));
            flare->instance_is_ready = FALSE;
        }

        if (ret != FCS_STATE_SUSPEND_PROCESS)
        {
            user->all_instances_were_suspended = FALSE;
        }

        user->active_flare->obj_stats.num_checked_states = user->active_flare->obj.i__num_checked_states;
        user->active_flare->obj_stats.num_states_in_collection = user->active_flare->obj.num_states_in_collection;
        user->iterations_board_started_at.num_checked_states += user->active_flare->obj_stats.num_checked_states - init_num_checked_states.num_checked_states;
        user->iterations_board_started_at.num_states_in_collection += user->active_flare->obj_stats.num_states_in_collection - init_num_checked_states.num_states_in_collection;
        user->init_num_checked_states = user->active_flare->obj_stats;

        if (user->ret_code == FCS_STATE_WAS_SOLVED)
        {
            user->trace_solution_state_locs = user->state_locs;

            flare->was_solution_traced = FALSE;
#define FLARE_MOVE_COUNT(idx) \
           (get_flare_move_count( \
               user, \
               &(instance_item->flares[idx]) \
           ))
            if (
                (instance_item->minimal_solution_flare_idx < 0)
                 ||
                (
                    FLARE_MOVE_COUNT(instance_item->minimal_solution_flare_idx)
                    >
                    FLARE_MOVE_COUNT(flare_idx)
                )
            )
            {
                instance_item->minimal_solution_flare_idx = flare_idx;
            }
#undef FLARE_MOVE_COUNT
            ret = user->ret_code = FCS_STATE_IS_NOT_SOLVEABLE;
        }
        else if (user->ret_code == FCS_STATE_IS_NOT_SOLVEABLE)
        {
            if (was_run_now)
            {
                fc_solve_recycle_instance(&(user->active_flare->obj));
                user->active_flare->instance_is_ready = TRUE;
            }
        }
        else if (user->ret_code == FCS_STATE_SUSPEND_PROCESS)
        {
            /*
             * First - check if we exceeded our limit. If so - we must terminate
             * and return now.
             * */
            if (((user->current_iterations_limit >= 0) &&
                (user->iterations_board_started_at.num_checked_states >=
                    user->current_iterations_limit)) ||
                (user->active_flare->obj.num_states_in_collection >=
                    user->active_flare->obj.effective_max_num_states_in_collection)
            )
            {
                /* Bug fix:
                 * We need to resume from the last flare in case we exceed
                 * the board iterations limit.
                 * */
                instance_item->current_plan_item_idx--;
                break;
            }

            ret = FCS_STATE_IS_NOT_SOLVEABLE;


            /*
             * Determine if we exceeded the instance-specific quota and if
             * so, designate it as unsolvable.
             * */
            if ((local_limit() >= 0) &&
                (user->active_flare->obj.i__num_checked_states >= local_limit())
               )
            {
                user->active_flare->obj_stats.num_checked_states =
                    user->active_flare->obj.i__num_checked_states;
                user->active_flare->obj_stats.num_states_in_collection =
                    user->active_flare->obj.num_states_in_collection;
                recycle_instance(user, user->current_instance_idx);
                user->current_instance_idx++;
                continue;
            }
            instance_item->all_plan_items_finished_so_far = 0;
        }
    } while (
        (user->current_instance_idx < user->num_instances) &&
        (ret == FCS_STATE_IS_NOT_SOLVEABLE)
    );

    return
    (
        user->all_instances_were_suspended ? FCS_STATE_SUSPEND_PROCESS : ret
    );
}

static GCC_INLINE fcs_flare_item_t * const calc_moves_flare(
    fcs_user_t * const user
)
{
    fcs_instance_item_t const * instance_item =
        get_current_instance_item(user);
    fcs_flare_item_t * const flare = &(instance_item->flares[instance_item->minimal_solution_flare_idx]);

    trace_flare_solution(user, flare);

    return flare;
}


int DLLEXPORT freecell_solver_user_get_next_move(
    void * const api_instance,
    fcs_move_t * const user_move
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    if (user->ret_code != FCS_STATE_WAS_SOLVED)
    {
        return 1;
    }
    fcs_flare_item_t * const flare = calc_moves_flare(user);
    if (flare->next_move == flare->moves_seq.num_moves)
    {
        return 1;
    }

    fcs_kv_state_t pass = FCS_STATE_keyval_pair_to_kv(&(user->running_state));

#if (!(defined(HARD_CODED_NUM_FREECELLS) && defined(HARD_CODED_NUM_STACKS) && defined(HARD_CODED_NUM_DECKS)))
    fc_solve_instance_t * const instance = &(user->active_flare->obj);
#endif

    fc_solve_apply_move(
        &(pass),
        NULL,
        user_move_to_internal_move(*user_move = flare->moves_seq.moves[flare->next_move++]),
        INSTANCE_FREECELLS_NUM,
        INSTANCE_STACKS_NUM,
        INSTANCE_DECKS_NUM
    );

    return 0;
}

DLLEXPORT char * freecell_solver_user_current_state_as_string(
    void * api_instance,
    int parseable_output,
    int canonized_order_output,
    int display_10_as_t
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    {
#if (!(defined(HARD_CODED_NUM_FREECELLS) && defined(HARD_CODED_NUM_STACKS) && defined(HARD_CODED_NUM_DECKS)))
        fc_solve_instance_t * const instance = &(user->active_flare->obj);
#endif

        return
            fc_solve_state_as_string(
                &(user->running_state.s),
                &(user->initial_state_locs),
                INSTANCE_FREECELLS_NUM,
                INSTANCE_STACKS_NUM,
                INSTANCE_DECKS_NUM,
                parseable_output,
                canonized_order_output,
                display_10_as_t
                );
    }
}

static void user_free_resources(
    fcs_user_t * const user
    )
{
    FLARES_LOOP_START()
    {
        const int ret_code = flare->ret_code;

        /*  TODO : for later It's possible two flares in a single-instance
         *  will be solved. Make sure the check is instance-wide.
         *  */
        if ((ret_code != FCS_STATE_NOT_BEGAN_YET) &&
            (ret_code != FCS_STATE_INVALID_STATE) &&
            (! flare->instance_is_ready))
        {
            fc_solve_finish_instance(&(flare->obj));
        }

        fc_solve_free_instance(&(flare->obj));

        flare->name[0] = '\0';

        if (flare->fc_pro_moves.moves)
        {
            fc_solve_moves_processed_free(&(flare->fc_pro_moves));
        }

        if (flare->moves_seq.moves)
        {
            free (flare->moves_seq.moves);
            flare->moves_seq.moves = NULL;
            flare->moves_seq.num_moves = 0;
        }
    }
    FLARES_LOOP_END_FLARES()
        free (instance_item->flares);
        if (instance_item->flares_plan_string)
        {
            free(instance_item->flares_plan_string);
        }
        if (instance_item->plan)
        {
            free (instance_item->plan);
        }
    FLARES_LOOP_END_INSTANCES()

    free(user->instances_list);

    if (user->state_string_copy != NULL)
    {
        free(user->state_string_copy);
        user->state_string_copy = NULL;
    }

    if (user->error_string)
    {
        free(user->error_string);
        user->error_string = NULL;
    }

    fc_solve_meta_compact_allocator_finish(&(user->meta_alloc));
}

void DLLEXPORT freecell_solver_user_free(
    void * api_instance
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    user_free_resources(user);

    free(user);
}

int DLLEXPORT freecell_solver_user_get_current_depth(
    void * const api_instance
    )
{
    const fcs_user_t * const user = (fcs_user_t *)api_instance;

    return (DFS_VAR(user->soft_thread, depth));
}

extern int DLLEXPORT freecell_solver_user_set_patsolve_x_param(
    void * const api_instance,
    const int position,
    const int x_param_val,
    char * * const error_string
    )
{
#ifndef FCS_DISABLE_PATSOLVE
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    fc_solve_soft_thread_t * const soft_thread = user->soft_thread;
    typeof(soft_thread->pats_scan) pats_scan = soft_thread->pats_scan;

    if (! pats_scan)
    {
        *error_string = strdup("Not using the \"patsolve\" scan.");
        return 1;
    }
    if ( (position < 0) || (position >= COUNT(pats_scan->pats_solve_params.x)))
    {
        *error_string = strdup("Position out of range.");
        return 2;
    }

    pats_scan->pats_solve_params.x[position] = x_param_val;
    pats_scan->cutoff = pats_scan->pats_solve_params.x[FC_SOLVE_PATS__NUM_X_PARAM - 1];
#endif
    return 0;
}

extern int DLLEXPORT freecell_solver_user_set_patsolve_y_param(
    void * api_instance,
    const int position,
    const double y_param_val,
    char * * error_string
    )
{
#ifndef FCS_DISABLE_PATSOLVE
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    fc_solve_soft_thread_t * const soft_thread = user->soft_thread;
    typeof(soft_thread->pats_scan) pats_scan = soft_thread->pats_scan;

    if (! pats_scan)
    {
        *error_string = strdup("Not using the \"patsolve\" scan.");
        return 1;
    }
    if ( (position < 0) || (position >= COUNT(pats_scan->pats_solve_params.y)))
    {
        *error_string = strdup("Position out of range.");
        return 2;
    }

    pats_scan->pats_solve_params.y[position] = y_param_val;
#endif
    return 0;
}

void DLLEXPORT freecell_solver_user_set_solving_method(
    void * const api_instance,
    const int method
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    fc_solve_soft_thread_t * const soft_thread = user->soft_thread;
    fcs_super_method_type_t super_method_type = FCS_SUPER_METHOD_BEFS_BRFS;

    if (method == FCS_METHOD_HARD_DFS)
    {
        return freecell_solver_user_set_solving_method(api_instance, FCS_METHOD_SOFT_DFS);
    }

    switch ((soft_thread->method = method))
    {
        case FCS_METHOD_RANDOM_DFS:
        case FCS_METHOD_SOFT_DFS:
        {
            super_method_type = FCS_SUPER_METHOD_DFS;
        }
        break;

#ifndef FCS_DISABLE_PATSOLVE
        case FCS_METHOD_PATSOLVE:
        {
            super_method_type = FCS_SUPER_METHOD_PATSOLVE;

            if (! soft_thread->pats_scan)
            {
                typeof(soft_thread->pats_scan) pats_scan
                    = soft_thread->pats_scan = SMALLOC1(soft_thread->pats_scan);
                fc_solve_pats__init_soft_thread(pats_scan,
                    HT_INSTANCE(soft_thread->hard_thread));

                pats_scan->to_stack = 1;

                pats_scan->pats_solve_params = (freecell_solver_pats__x_y_params_preset[FC_SOLVE_PATS__PARAM_PRESET__FreecellSpeed]);
                pats_scan->cutoff = pats_scan->pats_solve_params.x[FC_SOLVE_PATS__NUM_X_PARAM - 1];
            }
        }
        break;
#endif
    }

    soft_thread->super_method_type = super_method_type;
}

#ifndef FCS_FREECELL_ONLY
static GCC_INLINE void calc_variant_suit_mask_and_desired_suit_value(
    fc_solve_instance_t * const instance
)
{
    instance->game_variant_suit_mask = FCS_PATS__COLOR;
    instance->game_variant_desired_suit_value = FCS_PATS__COLOR;
    if ((GET_INSTANCE_SEQUENCES_ARE_BUILT_BY(instance) == FCS_SEQ_BUILT_BY_SUIT)) {
        instance->game_variant_suit_mask = FCS_PATS__SUIT;
        instance->game_variant_desired_suit_value = 0;
    }
}

static void apply_game_params_for_all_instances(
        fcs_user_t * const user
        )
{
    FLARES_LOOP_START()
    {
        typeof(&(flare->obj)) instance = &(flare->obj);
        instance->game_params = user->common_preset.game_params;
        calc_variant_suit_mask_and_desired_suit_value(instance);
    }
    FLARES_LOOP_END()

    return;
}

#endif

#ifndef HARD_CODED_NUM_FREECELLS

int DLLEXPORT freecell_solver_user_set_num_freecells(
    void * api_instance,
    int freecells_num
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    if ((freecells_num < 0) || (freecells_num > MAX_NUM_FREECELLS))
    {
        return 1;
    }

    user->common_preset.game_params.freecells_num = freecells_num;

    apply_game_params_for_all_instances(user);

    return 0;
}

#else

int DLLEXPORT freecell_solver_user_set_num_freecells(
    void * api_instance GCC_UNUSED,
    int freecells_num GCC_UNUSED
    )
{
    return 0;
}

#endif

#ifndef HARD_CODED_NUM_STACKS
int DLLEXPORT freecell_solver_user_set_num_stacks(
    void * api_instance,
    int stacks_num
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    if ((stacks_num < 0) || (stacks_num > MAX_NUM_STACKS))
    {
        return 1;
    }
    user->common_preset.game_params.stacks_num = stacks_num;
    apply_game_params_for_all_instances(user);

    return 0;
}

#else

int DLLEXPORT freecell_solver_user_set_num_stacks(
    void * api_instance GCC_UNUSED,
    int stacks_num GCC_UNUSED
    )
{
    return 0;
}

#endif

#ifndef HARD_CODED_NUM_DECKS
int DLLEXPORT freecell_solver_user_set_num_decks(
    void * api_instance,
    int decks_num
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    if ((decks_num < 0) || (decks_num > MAX_NUM_DECKS))
    {
        return 1;
    }

    user->common_preset.game_params.decks_num = decks_num;
    apply_game_params_for_all_instances(user);

    return 0;
}

#else

int DLLEXPORT freecell_solver_user_set_num_decks(
    void * api_instance GCC_UNUSED,
    int decks_num GCC_UNUSED
    )
{
    return 0;
}

#endif

int DLLEXPORT freecell_solver_user_set_game(
    void * const api_instance,
    const int freecells_num,
    const int stacks_num,
    const int decks_num,
    const int sequences_are_built_by,
    const int unlimited_sequence_move,
    const int empty_stacks_fill
)
{
    if (freecell_solver_user_set_num_freecells(api_instance, freecells_num))
    {
        return 1;
    }
    if (freecell_solver_user_set_num_stacks(api_instance, stacks_num))
    {
        return 2;
    }
    if (freecell_solver_user_set_num_decks(api_instance, decks_num))
    {
        return 3;
    }
    if (freecell_solver_user_set_sequences_are_built_by_type(api_instance, sequences_are_built_by))
    {
        return 4;
    }
    if (freecell_solver_user_set_sequence_move(api_instance, unlimited_sequence_move))
    {
        return 5;
    }
    if (freecell_solver_user_set_empty_stacks_filled_by(api_instance, empty_stacks_fill))
    {
        return 6;
    }

    return 0;
}

fcs_int_limit_t DLLEXPORT freecell_solver_user_get_num_times_long(
    void * api_instance
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    return user->iterations_board_started_at.num_checked_states + max(user->active_flare->obj_stats.num_checked_states, user->active_flare->obj.i__num_checked_states) - user->init_num_checked_states.num_checked_states;
}

int DLLEXPORT freecell_solver_user_get_num_times(void * const api_instance)
{
    return (int)freecell_solver_user_get_num_times_long(api_instance);
}

int DLLEXPORT freecell_solver_user_get_limit_iterations(void * const api_instance)
{
    const fcs_user_t * const user = (const fcs_user_t *)api_instance;

    return user->active_flare->obj.i__max_num_checked_states;
}

int DLLEXPORT freecell_solver_user_get_moves_left(void * const api_instance)
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    if (user->ret_code == FCS_STATE_WAS_SOLVED)
    {
        const fcs_flare_item_t * const flare = calc_moves_flare(user);
        return flare->moves_seq.num_moves - flare->next_move;
    }
    else
    {
        return 0;
    }
}

void DLLEXPORT freecell_solver_user_set_solution_optimization(
    void * const api_instance,
    const int optimize
)
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    STRUCT_SET_FLAG_TO(&(user->active_flare->obj), FCS_RUNTIME_OPTIMIZE_SOLUTION_PATH, optimize);
}

DLLEXPORT char * freecell_solver_user_move_to_string(
    const fcs_move_t move,
    const int standard_notation
    )
{
    return
        fc_solve_move_to_string_w_state(
            NULL, 4, 8, 1,
            move,
            (standard_notation == 2)?1:standard_notation
            );
}

DLLEXPORT char * freecell_solver_user_move_to_string_w_state(
    void * api_instance,
    fcs_move_t move,
    int standard_notation
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

#if (!(defined(HARD_CODED_NUM_FREECELLS) && defined(HARD_CODED_NUM_STACKS) && defined(HARD_CODED_NUM_DECKS)))
    fc_solve_instance_t * const instance = &(user->active_flare->obj);
#endif


    return
        fc_solve_move_to_string_w_state(
            &(user->running_state),
            INSTANCE_FREECELLS_NUM,
            INSTANCE_STACKS_NUM,
            INSTANCE_DECKS_NUM,
            move,
            standard_notation
            );
}

void DLLEXPORT freecell_solver_user_limit_depth(
    void * const api_instance GCC_UNUSED,
    const int max_depth GCC_UNUSED
)
{
#ifdef FC_SOLVE__WITH_MAX_DEPTH
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    user->active_flare->obj.max_depth = max_depth;
#endif
}

int DLLEXPORT freecell_solver_user_get_max_num_freecells(void)
{
    return MAX_NUM_FREECELLS;
}

int DLLEXPORT freecell_solver_user_get_max_num_stacks(void)
{
    return MAX_NUM_STACKS;
}

int DLLEXPORT freecell_solver_user_get_max_num_decks(void)
{
    return MAX_NUM_DECKS;
}


char * freecell_solver_user_get_invalid_state_error_string(
    void * const api_instance,
    const int print_ts
    )
{
    char string[80];

    fcs_user_t * const user = (fcs_user_t *)api_instance;

    const typeof (user->state_validity_ret) ret = user->state_validity_ret;
    switch (ret)
    {
        case FCS_STATE_VALIDITY__OK:
        return strdup("");

        case FCS_STATE_VALIDITY__EMPTY_SLOT:
        strcpy(string, "There's an empty slot in one of the stacks.");
        break;

        case FCS_STATE_VALIDITY__MISSING_CARD:
        case FCS_STATE_VALIDITY__EXTRA_CARD:
        {
            /*
             * user->state_validity_card is only valid for these states,
             * so we should call fc_solve_card_perl2user on there only.
             * */
            char card_str[4];
            fc_solve_card_perl2user(user->state_validity_card, card_str, print_ts);

            sprintf(string, "%s%s.",
                ((ret == FCS_STATE_VALIDITY__EXTRA_CARD)? "There's an extra card: " : "There's a missing card: "),
                card_str
            );
        }
        break;

        case FCS_STATE_VALIDITY__PREMATURE_END_OF_INPUT:
        strcpy(string, "Not enough input.");
        break;
    }
    return strdup(string);
}

int DLLEXPORT freecell_solver_user_set_sequences_are_built_by_type(
    void * api_instance,
    int sequences_are_built_by
    )
{
#ifndef FCS_FREECELL_ONLY
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    if ((sequences_are_built_by < 0) || (sequences_are_built_by > 2))
    {
        return 1;
    }

    user->common_preset.game_params.game_flags &= (~0x3);
    user->common_preset.game_params.game_flags |= sequences_are_built_by;

    apply_game_params_for_all_instances(user);
#endif

    return 0;
}

int DLLEXPORT freecell_solver_user_set_sequence_move(
    void * api_instance,
    int unlimited_sequence_move
    )
{
#ifndef FCS_FREECELL_ONLY
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    user->common_preset.game_params.game_flags &= (~(1 << 4));
    user->common_preset.game_params.game_flags |=
        ((unlimited_sequence_move != 0)<< 4);

    apply_game_params_for_all_instances(user);
#endif
    return 0;
}

int DLLEXPORT freecell_solver_user_set_empty_stacks_filled_by(
    void * api_instance,
    int empty_stacks_fill
    )
{
#ifndef FCS_FREECELL_ONLY
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    if ((empty_stacks_fill < 0) || (empty_stacks_fill > 2))
    {
        return 1;
    }

    user->common_preset.game_params.game_flags &= (~(0x3 << 2));
    user->common_preset.game_params.game_flags |=
        (empty_stacks_fill << 2);

    apply_game_params_for_all_instances(user);
#endif
    return 0;
}

int DLLEXPORT freecell_solver_user_set_a_star_weight(
    void * const api_instance,
    const int my_index,
    const double weight
)
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    if ((my_index < 0) || (my_index >= COUNT(BEFS_VAR(user->soft_thread, weighting.befs_weights))))
    {
        return 1;
    }
    if (weight < 0)
    {
        return 2;
    }

    BEFS_VAR(user->soft_thread, weighting.befs_weights)[my_index] = weight;

    return 0;

}


#ifdef FCS_COMPILE_DEBUG_FUNCTIONS
double DLLEXPORT fc_solve_user_INTERNAL_get_befs_weight(
    void * const api_instance,
    const int my_index
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    return BEFS_VAR(user->soft_thread, weighting.befs_weights)[my_index];
}

#endif

static void iter_handler_wrapper(
    void * const api_instance,
    const fcs_int_limit_t iter_num,
    const int depth,
    void * lp_instance GCC_UNUSED,
    fcs_kv_state_t * const ptr_state,
    const fcs_int_limit_t parent_iter_num
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    fcs_standalone_state_ptrs_t state_raw = {.key = ptr_state->key, .val = ptr_state->val};
#if 0
    state_raw.key = ptr_state->key;
    state_raw.val = ptr_state->val;
#endif
    fc_solve_init_locs(&(state_raw.locs));

#define CALL(func_ptr) (func_ptr) \
    ( \
      api_instance, \
      iter_num, \
      depth, \
      (void *)&state_raw, \
      parent_iter_num, \
      user->iter_handler_context \
    )

    if (user->long_iter_handler)
    {
        CALL(user->long_iter_handler);
    }
    else
    {
        CALL(user->iter_handler);
    }
#undef CALL
}

static GCC_INLINE void set_debug_iter_output_func_to_val(
    fcs_user_t * const user,
    const fcs_instance_debug_iter_output_func_t value
)
{
    FLARES_LOOP_START()
       flare->obj.debug_iter_output_func = value;
    FLARES_LOOP_END()
}


static GCC_INLINE void set_any_iter_handler(
    void * const api_instance,
    const freecell_solver_user_long_iter_handler_t long_iter_handler,
    const freecell_solver_user_iter_handler_t iter_handler,
    void * const iter_handler_context
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    user->long_iter_handler = long_iter_handler;
    user->iter_handler = iter_handler;

    fcs_instance_debug_iter_output_func_t cb = NULL;
    if (iter_handler || long_iter_handler)
    {
        user->iter_handler_context = iter_handler_context;
        cb = iter_handler_wrapper;
    }
    set_debug_iter_output_func_to_val(user, cb);
}

void DLLEXPORT freecell_solver_user_set_iter_handler_long(
    void * api_instance,
    freecell_solver_user_long_iter_handler_t long_iter_handler,
    void * iter_handler_context
    )
{
    return set_any_iter_handler(api_instance, long_iter_handler, NULL, iter_handler_context);
}



void DLLEXPORT freecell_solver_user_set_iter_handler(
    void * const api_instance,
    const freecell_solver_user_iter_handler_t iter_handler,
    void * const iter_handler_context
    )
{
    return set_any_iter_handler(api_instance, NULL, iter_handler, iter_handler_context);
}

#if (!(defined(HARD_CODED_NUM_FREECELLS) && defined(HARD_CODED_NUM_STACKS) && defined(HARD_CODED_NUM_DECKS)))
#define HARD_CODED_UNUSED
#else
#define HARD_CODED_UNUSED GCC_UNUSED
#endif

DLLEXPORT char * freecell_solver_user_iter_state_as_string(
    void * const api_instance HARD_CODED_UNUSED,
    void * const ptr_state_void,
    const int parseable_output,
    const int canonized_order_output,
    const int display_10_as_t
)
{
#if (!(defined(HARD_CODED_NUM_FREECELLS) && defined(HARD_CODED_NUM_STACKS) && defined(HARD_CODED_NUM_DECKS)))
    fcs_user_t * const user = (fcs_user_t *)api_instance;
    fc_solve_instance_t * const instance =
        &(user->active_flare->obj);
#endif

    return
        fc_solve_state_as_string(
            ((fcs_standalone_state_ptrs_t *)ptr_state_void)->key,
            &(((fcs_standalone_state_ptrs_t *)ptr_state_void)->locs),
            INSTANCE_FREECELLS_NUM,
            INSTANCE_STACKS_NUM,
            INSTANCE_DECKS_NUM,
            parseable_output,
            canonized_order_output,
            display_10_as_t
            );
}

void DLLEXPORT freecell_solver_user_set_random_seed(
    void * const api_instance,
    const int seed
)
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    fc_solve_rand_init(
            &(DFS_VAR(user->soft_thread, rand_gen)),
            (DFS_VAR(user->soft_thread, rand_seed) = seed)
            );
}

fcs_int_limit_t DLLEXPORT freecell_solver_user_get_num_states_in_collection_long(void * api_instance)
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    return user->iterations_board_started_at.num_states_in_collection + user->active_flare->obj_stats.num_states_in_collection - user->init_num_checked_states.num_states_in_collection;
}

int DLLEXPORT freecell_solver_user_get_num_states_in_collection(void * const api_instance)
{
    return (int)freecell_solver_user_get_num_states_in_collection_long(api_instance);
}

void DLLEXPORT freecell_solver_user_limit_num_states_in_collection_long(
    void * api_instance,
    fcs_int_limit_t max_num_states
    )
{
    fcs_user_t * user = (fcs_user_t*)api_instance;

    if (max_num_states < 0)
    {
        user->active_flare->obj.max_num_states_in_collection = -1;
        user->active_flare->obj.effective_max_num_states_in_collection
            = FCS_INT_LIMIT_MAX;
    }
    else
    {
        user->active_flare->obj.effective_max_num_states_in_collection =
            user->active_flare->obj.max_num_states_in_collection =
                max_num_states;
    }

    return;
}

void DLLEXPORT freecell_solver_user_limit_num_states_in_collection(
    void * const api_instance,
    const int max_num_states
    )
{
    return freecell_solver_user_limit_num_states_in_collection_long(api_instance, (fcs_int_limit_t)max_num_states);
}

DLLEXPORT extern void freecell_solver_set_stored_states_trimming_limit(
    void * const api_instance,
    const long max_num_states
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    if (max_num_states < 0)
    {
        user->active_flare->obj.trim_states_in_collection_from = -1;
        user->active_flare->obj.effective_trim_states_in_collection_from = LONG_MAX;
    }
    else
    {
        user->active_flare->obj.effective_trim_states_in_collection_from =
            user->active_flare->obj.trim_states_in_collection_from =
            max_num_states;
    }

    return;

}

int DLLEXPORT freecell_solver_user_next_soft_thread(
    void * const api_instance
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    fc_solve_soft_thread_t * const soft_thread = fc_solve_new_soft_thread(user->soft_thread->hard_thread);

    if (soft_thread == NULL)
    {
        return 1;
    }

    user->soft_thread = soft_thread;

    return 0;
}

extern void DLLEXPORT freecell_solver_user_set_soft_thread_step(
    void * const api_instance,
    const int num_checked_states_step
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    user->soft_thread->num_checked_states_step = num_checked_states_step;
}

int DLLEXPORT freecell_solver_user_next_hard_thread(
    void * const api_instance
    )
{
#ifdef FCS_SINGLE_HARD_THREAD
    return freecell_solver_user_next_soft_thread(api_instance);
#else
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    fc_solve_soft_thread_t * const soft_thread = fc_solve_new_hard_thread(&(user->active_flare->obj));

    if (soft_thread == NULL)
    {
        return 1;
    }

    user->soft_thread = soft_thread;

    return 0;
#endif
}

int DLLEXPORT freecell_solver_user_get_num_soft_threads_in_instance(
    void * const api_instance
    )
{
    const fcs_user_t * const user = (const fcs_user_t *)api_instance;

    return user->active_flare->obj.next_soft_thread_id;
}

void DLLEXPORT freecell_solver_user_set_calc_real_depth(
    void * const api_instance,
    const int calc_real_depth
)
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    STRUCT_SET_FLAG_TO(&(user->active_flare->obj), FCS_RUNTIME_CALC_REAL_DEPTH, calc_real_depth);
}

void DLLEXPORT freecell_solver_user_set_soft_thread_name(
    void * const api_instance,
    const freecell_solver_str_t name
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    strncpy (user->soft_thread->name, name, COUNT(user->soft_thread->name));
    user->soft_thread->name[COUNT(user->soft_thread->name)-1] = '\0';
}

void DLLEXPORT freecell_solver_user_set_flare_name(
    void * const api_instance,
    const freecell_solver_str_t name
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    fcs_instance_item_t * const instance_item = get_current_instance_item(user);

    fcs_flare_item_t * const flare = &(instance_item->flares[instance_item->num_flares-1]);

    strncpy(flare->name, name, COUNT(flare->name));
    flare->name[COUNT(flare->name)-1] = '\0';

    return;
}

int DLLEXPORT freecell_solver_user_set_hard_thread_prelude(
    void * const api_instance,
    const char * const prelude
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    fc_solve_hard_thread_t * const hard_thread = user->soft_thread->hard_thread;

    if (HT_FIELD(hard_thread, prelude_as_string) != NULL)
    {
        free (HT_FIELD(hard_thread, prelude_as_string));
    }
    HT_FIELD(hard_thread, prelude_as_string) = strdup(prelude);

    return 0;
}

int DLLEXPORT freecell_solver_user_set_flares_plan(
    void * const api_instance,
    const char * const flares_plan_string
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    fcs_instance_item_t * const instance_item = get_current_instance_item(user);

    if (instance_item->flares_plan_string)
    {
        free(instance_item->flares_plan_string);
    }

    instance_item->flares_plan_string =
        (flares_plan_string ? strdup(flares_plan_string) : NULL);

    instance_item->flares_plan_compiled = FALSE;

    return 0;
}

void DLLEXPORT freecell_solver_user_recycle(
    void * api_instance
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    for (int i = 0 ; i < user->num_instances ; i++)
    {
        recycle_instance(user, i);
    }
    /*
     * Removing because we are still interested to keep the current iterations
     * limit.
     * */
#if 0
    user->current_iterations_limit = -1;
#endif
    user->iterations_board_started_at = calc_initial_stats_t();
    if (user->state_string_copy != NULL)
    {
        free(user->state_string_copy);
        user->state_string_copy = NULL;
    }
}

int DLLEXPORT freecell_solver_user_set_optimization_scan_tests_order(
    void * const api_instance,
    const char * const tests_order,
    char * * const error_string
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    fc_solve_free_tests_order(&(user->active_flare->obj.opt_tests_order));

    STRUCT_CLEAR_FLAG(&(user->active_flare->obj), FCS_RUNTIME_OPT_TESTS_ORDER_WAS_SET );

    const int ret =
        fc_solve_apply_tests_order(
            &(user->active_flare->obj.opt_tests_order),
            tests_order,
            error_string
            );

    if (!ret)
    {
        STRUCT_TURN_ON_FLAG(&(user->active_flare->obj), FCS_RUNTIME_OPT_TESTS_ORDER_WAS_SET);
    }

    return ret;
}


extern int DLLEXPORT freecell_solver_user_set_pruning(
    void * api_instance,
    const char * pruning,
    char * * error_string
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    if (!strcmp(pruning, "r:tf"))
    {
        user->soft_thread->enable_pruning = TRUE;
    }
    else if (pruning[0] == '\0')
    {
        user->soft_thread->enable_pruning = FALSE;
    }
    else
    {
        *error_string = strdup("Unknown pruning value - must be \"r:tf\" or empty.");

        return 1;
    }

    return 0;
}


void DLLEXPORT freecell_solver_user_set_reparent_states(
    void * const api_instance,
    const int to_reparent_states
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    STRUCT_SET_FLAG_TO(&(user->active_flare->obj),
            FCS_RUNTIME_TO_REPARENT_STATES_PROTO, to_reparent_states);
}

void DLLEXPORT freecell_solver_user_set_scans_synergy(
    void * const api_instance,
    const int synergy
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    STRUCT_SET_FLAG_TO(&(user->active_flare->obj), FCS_RUNTIME_SCANS_SYNERGY, synergy);
}

int DLLEXPORT freecell_solver_user_next_instance(
    void * const api_instance
    )
{
    return user_next_instance((fcs_user_t *)api_instance);
}

static int user_next_flare(fcs_user_t * const user)
{
    fcs_instance_item_t * const instance_item = get_current_instance_item(user);

    instance_item->flares =
        SREALLOC( instance_item->flares, ++(instance_item->num_flares) );

    fcs_flare_item_t * const flare = &(instance_item->flares[instance_item->num_flares-1]);
    instance_item->limit = flare->limit = -1;

    user->active_flare = flare;
    fc_solve_alloc_instance(&(flare->obj), &(user->meta_alloc));

    /*
     * Switch the soft_thread variable so it won't refer to the old
     * instance
     * */
    user->soft_thread =
        fc_solve_instance_get_first_soft_thread(&(user->active_flare->obj));

#ifndef FCS_FREECELL_ONLY
    fc_solve_apply_preset_by_ptr(&(flare->obj), &(user->common_preset));
    calc_variant_suit_mask_and_desired_suit_value(&(flare->obj));
#endif

    user->ret_code = flare->ret_code =
        FCS_STATE_NOT_BEGAN_YET;

    flare->obj.debug_iter_output_func =
        ((user->iter_handler || user->long_iter_handler)
         ? iter_handler_wrapper
         : NULL
        );
    flare->obj.debug_iter_output_context = user;
    flare->moves_seq.num_moves = 0;
    flare->moves_seq.moves = NULL;

    flare->name[0] = '\0';
    flare->fc_pro_moves.moves = NULL;
    flare->instance_is_ready = TRUE;
    flare->obj_stats = calc_initial_stats_t();

    return 0;
}

static int user_next_instance(
    fcs_user_t * const user
    )
{
    user->instances_list = SREALLOC(
        user->instances_list, ++user->num_instances
    );

    user->current_instance_idx = user->num_instances-1;

    fcs_instance_item_t * const instance_item = get_current_instance_item(user);
    instance_item->num_flares = 0;
    instance_item->flares = NULL;
    instance_item->plan = NULL;
    instance_item->num_plan_items = 0;
    instance_item->flares_plan_string = NULL;
    instance_item->flares_plan_compiled = FALSE;

    instance_item->current_plan_item_idx = 0;
    instance_item->minimal_solution_flare_idx = -1;
    instance_item->all_plan_items_finished_so_far = 1;

    /* ret_code and limit are set at user_next_flare(). */

    return user_next_flare(user);
}

int DLLEXPORT freecell_solver_user_next_flare(
    void * const api_instance
    )
{
    return user_next_flare((fcs_user_t *)api_instance);
}


int DLLEXPORT freecell_solver_user_reset(void * const api_instance)
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    user_free_resources(user);

    user_initialize(user);

    return 0;
}

DLLEXPORT const char * freecell_solver_user_get_lib_version(
    void * api_instance GCC_UNUSED
    )
{
    return VERSION;
}

DLLEXPORT const char * freecell_solver_user_get_current_soft_thread_name(
    void * const api_instance
    )
{
    const fcs_user_t * const user = (const fcs_user_t *)api_instance;

#ifdef FCS_SINGLE_HARD_THREAD
    const fc_solve_hard_thread_t * const hard_thread = &(user->active_flare->obj);
#else
    const fc_solve_hard_thread_t * const hard_thread = user->active_flare->obj.current_hard_thread;
#endif

    return HT_FIELD(hard_thread, soft_threads)[HT_FIELD(hard_thread, st_idx)].name;
}

DLLEXPORT const char * freecell_solver_user_get_last_error_string(
    void * api_instance
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    return user->error_string;
}


int DLLEXPORT freecell_solver_user_set_cache_limit(
    void * const api_instance,
    const long limit
    )
{
#ifndef FCS_RCS_STATES
    return 0;
#else
    if (limit <= 0)
    {
        return -1;
    }
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    FLARES_LOOP_START()
        flare->obj.rcs_states_cache.max_num_elements_in_cache = limit;
    FLARES_LOOP_END()

    return 0;
#endif
}


int DLLEXPORT freecell_solver_user_get_moves_sequence(
    void * const api_instance,
    fcs_moves_sequence_t * const moves_seq
)
{
    const fcs_user_t * const user = (const fcs_user_t *)api_instance;

    if (user->ret_code != FCS_STATE_WAS_SOLVED)
    {
        return -2;
    }

    const fcs_instance_item_t * const instance_item = get_current_instance_item(user);
    const fcs_flare_item_t * const flare = &(instance_item->flares[instance_item->minimal_solution_flare_idx]);

    moves_seq->moves = memdup(
        flare->moves_seq.moves,
        (sizeof(flare->moves_seq.moves[0])
        * (moves_seq->num_moves = flare->moves_seq.num_moves))
    );

    return 0;
}

DLLEXPORT extern int freecell_solver_user_set_flares_choice(
    void * api_instance,
    const char * const new_flares_choice_string
)
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    if (!strcmp(new_flares_choice_string, "fc_solve"))
    {
        user->flares_choice = FLARES_CHOICE_FC_SOLVE_SOLUTION_LEN;
    }
    else if (!strcmp(new_flares_choice_string, "fcpro"))
    {
        user->flares_choice = FLARES_CHOICE_FCPRO_SOLUTION_LEN;
    }
    else
    {
        return -1;
    }

    return 0;
}

DLLEXPORT extern void freecell_solver_user_set_flares_iters_factor(
    void * const api_instance,
    const double new_factor
)
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    user->flares_iters_factor = new_factor;
}

#ifdef FCS_COMPILE_DEBUG_FUNCTIONS

int DLLEXPORT fc_solve_user_INTERNAL_compile_all_flares_plans(
    void * api_instance,
    int * instance_list_index,
    char * * error_string
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    return user_compile_all_flares_plans(user, instance_list_index, error_string);
}

int DLLEXPORT fc_solve_user_INTERNAL_get_flares_plan_num_items(
    void * api_instance
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    return get_current_instance_item(user)->num_plan_items;
}

const DLLEXPORT char * fc_solve_user_INTERNAL_get_flares_plan_item_type(
    void * api_instance,
    int item_idx
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    switch (get_current_instance_item(user)->plan[item_idx].type)
    {
        case FLARES_PLAN_RUN_INDEFINITELY:
            return "RunIndef";
            break;
        case FLARES_PLAN_RUN_COUNT_ITERS:
            return "Run";
            break;
        case FLARES_PLAN_CHECKPOINT:
            return "CP";
            break;
        default:
            fprintf(stderr, "%s\n",
                    "Unknown flares plan item type. Something is Wrong on the Internet."
                   );
            exit(-1);
    }
}

int DLLEXPORT fc_solve_user_INTERNAL_get_flares_plan_item_flare_idx(
    void * api_instance,
    int item_idx
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    return get_current_instance_item(user)->plan[item_idx].flare_idx;
}

int DLLEXPORT fc_solve_user_INTERNAL_get_flares_plan_item_iters_count(
    void * api_instance,
    int item_idx
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    return get_current_instance_item(user)->plan[item_idx].count_iters;
}

int DLLEXPORT fc_solve_user_INTERNAL_get_num_by_depth_tests_order(
    void * api_instance
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    return user->soft_thread->by_depth_tests_order.num;
}

int DLLEXPORT fc_solve_user_INTERNAL_get_by_depth_tests_max_depth(
    void * api_instance,
    int depth_idx
    )
{
    fcs_user_t * const user = (fcs_user_t *)api_instance;

    return user->soft_thread->by_depth_tests_order.by_depth_tests[depth_idx].max_depth;
}

#endif

