/*
 * Copyright (c) 2000 Shlomi Fish
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
 * config.h - Configuration file for Freecell Solver
 *
 * config.h was auto-generated from config.h.in . Do not modify directly
*/

#ifndef FC_SOLVE__CONFIG_H
#define FC_SOLVE__CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* #undef DEBUG_STATES */
/* #undef COMPACT_STATES */
#define INDIRECT_STACK_STATES

/* #undef FCS_RCS_STATES */

/* #undef CARD_DEBUG_PRES */

/*
 * Define this macro if the C compiler supports the keyword inline or
 * a similar keyword that was found by Autoconf (and defined as inline).
 * */
#define HAVE_C_INLINE

#define FCS_INLINE_KEYWORD inline

/*
    The sort margin size for the previous states array.
*/
#define PREV_STATES_SORT_MARGIN 32

/*
    The amount the pack pointers array grows by. Shouldn't be too high
    because it doesn't happen too often.
*/
#define IA_STATE_PACKS_GROW_BY 32

/* The size of a single pack in alloc.c/alloc.h measured in 1024 chars. */
#define FCS_IA_PACK_SIZE 64

#ifndef FCS_FREECELL_ONLY
#define FCS_FREECELL_ONLY
#endif

#define FCS_DISABLE_SIMPLE_SIMON

#define FCS_SINGLE_HARD_THREAD

/*
 * This macro disables the flipping card logic. The flipping card logic
 * was an experimental feature that aimed to give Freecell Solver the
 * ability to solve Gypsy and Klondike-like games. In practice, this
 * didn't happen due to other technical difficulties.
 * */
/*
 * This flag used to slow things down a little:
 *
 * http://tech.groups.yahoo.com/group/fc-solve-discuss/message/941
 *
 * However, now it seems that it makes the program a little faster, as it
 * should, so you should enable it.
 * */
#define FCS_WITHOUT_CARD_FLIPPING

/*
 * Get rid of the visited_iter counter on each state's extra_info. It is
 * used a little for debugging, but otherwise is not needed for the run-time
 * operation.
 * */
#define FCS_WITHOUT_VISITED_ITER

/*
 * Disable the patsolve code at compile time in case it isn't wanted.
 * */
/* #undef FCS_DISABLE_PATSOLVE */
/*
 * Get rid of the "depth" field on each state's extra_info. It is
 * used a little, but otherwise is not needed for the run-time
 * operation.
 * */
#define FCS_WITHOUT_DEPTH_FIELD

/*
 * This flag controls a hash behaviour. It seems to improve things somewhat.
 * */
#define FCS_INLINED_HASH_COMPARISON 1

#define FCS_INT_BIT_SIZE_LOG2 5

#define FCS_WITH_CONTEXT_VARIABLE

/* This is an integer that specifies the maximal size of identifiers
 * in chars/bytes. */
#define FCS_MAX_IDENT_LEN 10

/*
 * The size of void*.
 */
#define SIZEOF_VOID_P 8

/*
 * This flag determines whether we are using the more compact but slower,
 * internal move tokens.
 *  */
/* #undef FCS_USE_COMPACT_MOVE_TOKENS */

#define FCS_HARD_CODED_NUM_FCS_FOR_FREECELL_ONLY 4

#ifndef FCS_HARD_CODED_NUM_FCS_FOR_FREECELL_ONLY
#define FCS_HARD_CODED_NUM_FCS_FOR_FREECELL_ONLY 4
#endif

/*
 * The maximal number of Freecells. For efficiency's sake it should be a
 * multiple of 4.
 * */

#ifdef FCS_FREECELL_ONLY
#define HARD_CODED_NUM_FREECELLS FCS_HARD_CODED_NUM_FCS_FOR_FREECELL_ONLY
#define HARD_CODED_NUM_STACKS 8
#define HARD_CODED_NUM_DECKS 1
#ifndef FCS_DISABLE_SIMPLE_SIMON
#define FCS_DISABLE_SIMPLE_SIMON
#endif

#if 0
#ifndef FCS_WITHOUT_CARD_FLIPPING
#define FCS_WITHOUT_CARD_FLIPPING
#endif
#endif

#endif

#ifdef HARD_CODED_NUM_FREECELLS
#define MAX_NUM_FREECELLS HARD_CODED_NUM_FREECELLS
#else
#define MAX_NUM_FREECELLS 8
#endif

/*
 * The maximal number of Stacks. For efficiency's sake it should be a
 * multiple of 4.
 * */

#ifdef HARD_CODED_NUM_STACKS
#define MAX_NUM_STACKS HARD_CODED_NUM_STACKS
#else
#define MAX_NUM_STACKS 10
#endif
/*
 * The maximal number of initial cards that can be found in a stack.
 * */
#define MAX_NUM_INITIAL_CARDS_IN_A_STACK 8

#ifdef HARD_CODED_NUM_DECKS
#define MAX_NUM_DECKS HARD_CODED_NUM_DECKS
#else
#define MAX_NUM_DECKS 2
#endif


#define FCS_STATE_STORAGE_INDIRECT 0
#define FCS_STATE_STORAGE_INTERNAL_HASH 1
#define FCS_STATE_STORAGE_LIBAVL2_TREE 2
#define FCS_STATE_STORAGE_LIBREDBLACK_TREE 3
#define FCS_STATE_STORAGE_GLIB_TREE 4
#define FCS_STATE_STORAGE_GLIB_HASH 5
#define FCS_STATE_STORAGE_DB_FILE 6
#define FCS_STATE_STORAGE_JUDY 7
#define FCS_STATE_STORAGE_GOOGLE_DENSE_HASH 8
#define FCS_STATE_STORAGE_KAZ_TREE 9

#define FCS_STACK_STORAGE_NULL (-1)
#define FCS_STACK_STORAGE_INTERNAL_HASH 0
#define FCS_STACK_STORAGE_LIBAVL2_TREE 1
#define FCS_STACK_STORAGE_LIBREDBLACK_TREE 2
#define FCS_STACK_STORAGE_GLIB_TREE 3
#define FCS_STACK_STORAGE_GLIB_HASH 4
#define FCS_STACK_STORAGE_JUDY 5
#define FCS_STACK_STORAGE_GOOGLE_DENSE_HASH 6

#define FCS_STATE_STORAGE FCS_STATE_STORAGE_INTERNAL_HASH
#define FCS_STACK_STORAGE FCS_STACK_STORAGE_INTERNAL_HASH

#define FCS_RCS_CACHE_STORAGE_JUDY 0
#define FCS_RCS_CACHE_STORAGE_KAZ_TREE 1

#define FCS_RCS_CACHE_STORAGE FCS_RCS_CACHE_STORAGE_KAZ_TREE

/* Define to 1 if you have the `m' library (-lm). */
/* #undef HAVE_LIBM */

#define HAVE_STRNDUP

#define HAVE_STRNCASECMP

/* Name of package */
#define PACKAGE ""

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Version number of package */
#define VERSION "3.27.0"

#ifndef FCS_INT_BIT_SIZE_LOG2
#define FCS_INT_BIT_SIZE_LOG2 ((sizeof(int) == 8) ? 6 : (sizeof(int) == 2) ? 4 : 5)
#endif

#define FCS_WHICH_STATES_GOOGLE_HASH__DENSE  0
#define FCS_WHICH_STATES_GOOGLE_HASH__SPARSE 1

#define FCS_WHICH_COLUMNS_GOOGLE_HASH__DENSE  0
#define FCS_WHICH_COLUMNS_GOOGLE_HASH__SPARSE 1

#define FCS_WHICH_COLUMNS_GOOGLE_HASH FCS_WHICH_COLUMNS_GOOGLE_HASH__SPARSE
#define FCS_WHICH_STATES_GOOGLE_HASH FCS_WHICH_STATES_GOOGLE_HASH__SPARSE

#if defined(FCS_RCS_STATES) && (!defined(COMPACT_STATES) )
#error FCS_RCS_STATES requires COMPACT_STATES
#endif

#ifdef __cplusplus
}
#endif

#endif    /* #ifndef FC_SOLVE__CONFIG_H */

