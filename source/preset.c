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
 * preset.c - game presets management for Freecell Solver
 *
 */

#define BUILDING_DLL 1

#include <string.h>
#include <stdlib.h>

#include "instance.h"
#include "preset.h"
#include "move_funcs_order.h"

#include "inline.h"
#include "count.h"

enum
{
    FCS_PRESET_BAKERS_DOZEN,
    FCS_PRESET_BAKERS_GAME,
    FCS_PRESET_CRUEL,
    FCS_PRESET_DER_KATZENSCHWANZ,
    FCS_PRESET_DIE_SCHLANGE,
    FCS_PRESET_EIGHT_OFF,
    FCS_PRESET_FAN,
    FCS_PRESET_FORECELL,
    FCS_PRESET_FREECELL,
    FCS_PRESET_GOOD_MEASURE,
    FCS_PRESET_KINGS_ONLY_BAKERS_GAME,
    FCS_PRESET_RELAXED_FREECELL,
    FCS_PRESET_RELAXED_SEAHAVEN_TOWERS,
    FCS_PRESET_SEAHAVEN_TOWERS,
    FCS_PRESET_SIMPLE_SIMON,
    FCS_PRESET_BELEAGUERED_CASTLE
};

#define MAKE_GAME_PARAMS(freecells_num, stacks_num, decks_num, seqs_build_by, seq_move, empty_stacks_fill) \
    { freecells_num, stacks_num, decks_num, (seqs_build_by | (empty_stacks_fill << 2) | (seq_move << 4))}

static const fcs_preset_t fcs_presets[16] =
{
    {
        FCS_PRESET_BAKERS_DOZEN,
        MAKE_GAME_PARAMS(
            0,
            13,
            1,

            FCS_SEQ_BUILT_BY_RANK,
            0,
            FCS_ES_FILLED_BY_NONE
        ),

        "0123456789",
        "0123456789"
    },
    {
        FCS_PRESET_BAKERS_GAME,
        MAKE_GAME_PARAMS(
            4,
            8,
            1,

            FCS_SEQ_BUILT_BY_SUIT,
            0,
            FCS_ES_FILLED_BY_ANY_CARD
        ),

        "[01][23456789]",
        "0123456789"
    },
    {
        FCS_PRESET_BELEAGUERED_CASTLE,
        MAKE_GAME_PARAMS(
            0,
            8,
            1,

            FCS_SEQ_BUILT_BY_RANK,
            0,
            FCS_ES_FILLED_BY_ANY_CARD
        ),

        "[01][23456789]",
        "0123456789"
    },
    {
        FCS_PRESET_CRUEL,
        MAKE_GAME_PARAMS(
            0,
            12,
            1,

            FCS_SEQ_BUILT_BY_SUIT,
            0,
            FCS_ES_FILLED_BY_NONE
        ),

        "0123456789",
        "0123456789"
    },
    {
        FCS_PRESET_DER_KATZENSCHWANZ,
        MAKE_GAME_PARAMS(
            8,
            9,
            2,

            FCS_SEQ_BUILT_BY_ALTERNATE_COLOR,
            1,
            FCS_ES_FILLED_BY_NONE
        ),

        "[01][23456789]",
        "0123456789"
    },
    {
        FCS_PRESET_DIE_SCHLANGE,
        MAKE_GAME_PARAMS(
            8,
            9,
            2,

            FCS_SEQ_BUILT_BY_ALTERNATE_COLOR,
            0,
            FCS_ES_FILLED_BY_NONE
        ),

        "[01][23456789]",
        "0123456789"
    },
    {
        FCS_PRESET_EIGHT_OFF,
        MAKE_GAME_PARAMS(
            8,
            8,
            1,

            FCS_SEQ_BUILT_BY_SUIT,
            0,
            FCS_ES_FILLED_BY_KINGS_ONLY
        ),

        "[01][23456789]",
        "0123456789"
    },
    {
        FCS_PRESET_FAN,
        MAKE_GAME_PARAMS(
            0,
            18,
            1,

            FCS_SEQ_BUILT_BY_SUIT,
            0,
            FCS_ES_FILLED_BY_KINGS_ONLY
        ),

        "[01][23456789]",
        "0123456789"
    },
    {
        FCS_PRESET_FORECELL,
        MAKE_GAME_PARAMS(
            4,
            8,
            1,

            FCS_SEQ_BUILT_BY_ALTERNATE_COLOR,
            0,
            FCS_ES_FILLED_BY_KINGS_ONLY
        ),

        "[01][23456789]",
        "0123456789"
    },
    {
        FCS_PRESET_FREECELL,
        MAKE_GAME_PARAMS(
            4,
            8,
            1,

            FCS_SEQ_BUILT_BY_ALTERNATE_COLOR,
            0,
            FCS_ES_FILLED_BY_ANY_CARD
        ),

        "[01][23456789]",
        "0123456789"
    },
    {
        FCS_PRESET_GOOD_MEASURE,
        MAKE_GAME_PARAMS(
            0,
            10,
            1,

            FCS_SEQ_BUILT_BY_RANK,
            0,
            FCS_ES_FILLED_BY_NONE
        ),

        "0123456789",
        "0123456789"
    },
    {
        FCS_PRESET_KINGS_ONLY_BAKERS_GAME,
        MAKE_GAME_PARAMS(
            4,
            8,
            1,

            FCS_SEQ_BUILT_BY_SUIT,
            0,
            FCS_ES_FILLED_BY_KINGS_ONLY
        ),

        "[01][23456789]",
        "0123456789"
    },
    {
        FCS_PRESET_RELAXED_FREECELL,
        MAKE_GAME_PARAMS(
            4,
            8,
            1,

            FCS_SEQ_BUILT_BY_ALTERNATE_COLOR,
            1,
            FCS_ES_FILLED_BY_ANY_CARD
        ),

        "[01][23456789]",
        "0123456789"
    },
    {
        FCS_PRESET_RELAXED_SEAHAVEN_TOWERS,
        MAKE_GAME_PARAMS(
            4,
            10,
            1,

            FCS_SEQ_BUILT_BY_SUIT,
            1,
            FCS_ES_FILLED_BY_KINGS_ONLY
        ),

        "[01][23456789]",
        "0123456789"
    },
    {
        FCS_PRESET_SEAHAVEN_TOWERS,
        MAKE_GAME_PARAMS(
            4,
            10,
            1,

            FCS_SEQ_BUILT_BY_SUIT,
            0,
            FCS_ES_FILLED_BY_KINGS_ONLY
        ),

        "[01][23456789]",
        "0123456789"
    },
    {
        FCS_PRESET_SIMPLE_SIMON,
        MAKE_GAME_PARAMS(
            0,
            10,
            1,

            FCS_SEQ_BUILT_BY_SUIT,
            0,
            FCS_ES_FILLED_BY_ANY_CARD
        ),

        "abcdefgh",
        "abcdefgh"
    }
};

typedef struct
{
    const char name[32];
    int preset_id;
} fcs_preset_name_t;

static const fcs_preset_name_t fcs_preset_names[23] =
{
    {
        "bakers_dozen",
        FCS_PRESET_BAKERS_DOZEN,
    },
    {
        "bakers_game",
        FCS_PRESET_BAKERS_GAME,
    },
    {
        "beleaguered_castle",
        FCS_PRESET_BELEAGUERED_CASTLE,
    },
    {
        "citadel",
        FCS_PRESET_BELEAGUERED_CASTLE,
    },
    {
        "cruel",
        FCS_PRESET_CRUEL,
    },
    {
        "der_katzenschwanz",
        FCS_PRESET_DER_KATZENSCHWANZ,
    },
    {
        "der_katz",
        FCS_PRESET_DER_KATZENSCHWANZ,
    },
    {
        "die_schlange",
        FCS_PRESET_DIE_SCHLANGE,
    },
    {
        "eight_off",
        FCS_PRESET_EIGHT_OFF,
    },
    {
        "fan",
        FCS_PRESET_FAN,
    },
    {
        "forecell",
        FCS_PRESET_FORECELL,
    },
    {
        "freecell",
        FCS_PRESET_FREECELL,
    },
    {
        "good_measure",
        FCS_PRESET_GOOD_MEASURE,
    },
    {
        "ko_bakers_game",
        FCS_PRESET_KINGS_ONLY_BAKERS_GAME,
    },
    {
        "kings_only_bakers_game",
        FCS_PRESET_KINGS_ONLY_BAKERS_GAME,
    },
    {
        "relaxed_freecell",
        FCS_PRESET_RELAXED_FREECELL,
    },
    {
        "relaxed_seahaven_towers",
        FCS_PRESET_RELAXED_SEAHAVEN_TOWERS,
    },
    {
        "relaxed_seahaven",
        FCS_PRESET_RELAXED_SEAHAVEN_TOWERS,
    },
    {
        "seahaven_towers",
        FCS_PRESET_SEAHAVEN_TOWERS,
    },
    {
        "seahaven",
        FCS_PRESET_SEAHAVEN_TOWERS,
    },
    {
        "simple_simon",
        FCS_PRESET_SIMPLE_SIMON,
    },
    {
        "streets_and_alleys",
        FCS_PRESET_BELEAGUERED_CASTLE,
    },
};

#define NUM_PRESETS ( (int) COUNT(fcs_preset_names) )

static GCC_INLINE const int fcs_get_preset_id_by_name(
    const char * const name
)
{
    for (int i=0 ; i < NUM_PRESETS ; i++)
    {
        if (!strcmp(name, fcs_preset_names[i].name))
        {
            return fcs_preset_names[i].preset_id;
        }
    }

    return -1;
}

const fc_solve_preset_ret_code_t fc_solve_apply_preset_by_ptr(
    fc_solve_instance_t * const instance,
    const fcs_preset_t * const preset_ptr
        )
{
    char * no_use;

#define preset (*preset_ptr)
    if (preset.game_params.freecells_num > MAX_NUM_FREECELLS)
    {
        return FCS_PRESET_CODE_FREECELLS_EXCEED_MAX;
    }
    if (preset.game_params.stacks_num > MAX_NUM_STACKS)
    {
        return FCS_PRESET_CODE_STACKS_EXCEED_MAX;
    }
    if (preset.game_params.decks_num > MAX_NUM_DECKS)
    {
        return FCS_PRESET_CODE_DECKS_EXCEED_MAX;
    }

    instance->game_params = preset.game_params;

    /*
     * This code makes sure that all the tests in all the existing
     * soft threads are acceptable by the new preset.
     * */

#ifdef FCS_SINGLE_HARD_THREAD
    if (instance->next_soft_thread_id)
#endif
    {
        char test_name[2] = {0};

        HT_LOOP_START()
        {
            ST_LOOP_START()
            {
                /* Check every test */

                fcs_by_depth_tests_order_t * const by_depth_tests_order =
                    soft_thread->by_depth_tests_order.by_depth_tests;

                for (int depth_idx = 0 ;
                    depth_idx < soft_thread->by_depth_tests_order.num ;
                    depth_idx++)
                {
                    for (int group_idx = 0 ; group_idx < by_depth_tests_order[depth_idx].tests_order.num_groups ; group_idx++)
                    {
                    const int * const tests_order_tests = by_depth_tests_order[depth_idx].tests_order.groups[group_idx].order_group_tests;
                    const int tests_order_num = by_depth_tests_order[depth_idx].tests_order.groups[group_idx].num;

                for (int num_valid_tests=0 ; num_valid_tests < tests_order_num ; num_valid_tests++)
                {
                    const char * s;
                    for (s = preset.allowed_tests; *s != '\0' ; s++)
                    {
                        test_name[0] = *s;
                        /* Check if this test corresponds to this character */
                        if (tests_order_tests[num_valid_tests] == (fc_solve_string_to_test_num(test_name) % FCS_MOVE_FUNCS_NUM))
                        {
                            break;
                        }
                    }
                    /* If the end of the string was reached, it means
                     * this test is unacceptable by this preset. */
                    if (*s == '\0')
                    {
                        fc_solve_apply_tests_order(
                            &(by_depth_tests_order[depth_idx].tests_order),
                            preset.tests_order,
                            &no_use
                        );
                        break;
                    }
                }
                    }
                }
            }
        }
    }

    /* Assign the master tests order */

    {
        fc_solve_apply_tests_order(
            &(instance->instance_tests_order),
            preset.tests_order,
            &no_use);
    }
#undef preset
    return FCS_PRESET_CODE_OK;
}

static GCC_INLINE const fc_solve_preset_ret_code_t fcs_get_preset_by_id(
    const int preset_id,
    const fcs_preset_t * * const preset_ptr
    )
{
    for (int preset_index=0 ; preset_index < NUM_PRESETS ; preset_index++)
    {
        if (fcs_presets[preset_index].preset_id == preset_id)
        {
            *preset_ptr = &(fcs_presets[preset_index]);
            return FCS_PRESET_CODE_OK;
        }
    }

    return FCS_PRESET_CODE_NOT_FOUND;
}

const fc_solve_preset_ret_code_t fc_solve_get_preset_by_name(
    const char * const name,
    const fcs_preset_t * * const preset_ptr
    )
{
    return fcs_get_preset_by_id(
        fcs_get_preset_id_by_name(name),
        preset_ptr
    );
}

