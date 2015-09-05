/* Copyright (c) 2000 Justin Heyes Jones
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
 * pqueue.h - header file for the priority queue implementation.
 *
 * Originally written by Justin-Heyes Jones
 * Modified by Shlomi Fish, 2000
 *
 * This file is available under either the MIT/X11 license (see above)
 * or the public domain.
 *
 * Check out Justin-Heyes Jones' A* page from which this code has
 * originated:
 *
 * http://www.heyes-jones.com/astar.html
*/

#ifndef FC_SOLVE__PQUEUE_H
#define FC_SOLVE__PQUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>

#include "state.h"

#include "inline.h"
#include "bool.h"

/* manage a priority queue as a heap
   the heap is implemented as a fixed size array of pointers to your data */

#define FC_SOLVE_PQUEUE_MaxRating INT_MAX

#define fcs_pq_rating(elem) ((elem).rating)

typedef int pq_rating_t;

typedef struct
{
    fcs_collectible_state_t * val;
    pq_rating_t rating;
} pq_element_t;

typedef struct
{
    int max_size;
    int CurrentSize;
    pq_element_t * Elements; /* pointer to void pointers */
} PQUEUE;

/* given an index to any element in a binary tree stored in a linear array with the root at 1 and
   a "sentinel" value at 0 these macros are useful in making the code clearer */

/* the parent is always given by index/2 */
#define PQ_PARENT_INDEX(i) ((i)>>1)
#define PQ_FIRST_ENTRY (1)

/* left and right children are index * 2 and (index * 2) +1 respectively */
#define PQ_LEFT_CHILD_INDEX(i) ((i)<<1)
/* initialise the priority queue with a maximum size of maxelements. maxrating is the highest or lowest value of an
   entry in the pqueue depending on whether it is ascending or descending respectively. Finally the bool32 tells you whether
   the list is sorted ascending or descending... */

static GCC_INLINE void fc_solve_pq_init(
    PQUEUE * const pq,
    const int max_size
    )
{
    pq->max_size = max_size;

    pq->CurrentSize = 0;

    pq->Elements = SMALLOC(pq->Elements, max_size + 1 );
}

static GCC_INLINE void fc_solve_PQueueFree( PQUEUE *pq )
{
    free( pq->Elements );
    pq->Elements = NULL;
}


#include <stdio.h>
#include <stdlib.h>

/* join a priority queue
   returns TRUE if succesful, FALSE if fails. (You fail by filling the pqueue.)
   fcs_pq_rating is a function which returns the rating of the item you're adding for sorting purposes */

static GCC_INLINE void fc_solve_pq_push(
        PQUEUE * const pq,
        fcs_collectible_state_t * const val,
        const pq_rating_t r
        )
{
    unsigned int i;
    pq_element_t * Elements = pq->Elements;

    int CurrentSize = pq->CurrentSize;

    if (CurrentSize == pq->max_size )
    {
        pq->Elements = Elements = (pq_element_t *)SREALLOC( Elements, (pq->max_size += 256)+1);
    }

    {
        /* set i to the first unused element and increment CurrentSize */

        i = (++CurrentSize);

        /* while the parent of the space we're putting the new node into is worse than
           our new node, swap the space with the worse node. We keep doing that until we
           get to a worse node or until we get to the top

           note that we also can sort so that the minimum elements bubble up so we need to loops
           with the comparison operator flipped... */

        {

            while( ( i==PQ_FIRST_ENTRY ?
                     (FC_SOLVE_PQUEUE_MaxRating) /* return biggest possible rating if first element */
                     :
                     (fcs_pq_rating(Elements[ PQ_PARENT_INDEX(i) ]) )
                   )
                   < r
                 )
            {
                Elements[ i ] = Elements[ PQ_PARENT_INDEX(i) ];

                i = PQ_PARENT_INDEX(i);
            }
        }

        /* then add the element at the space we created. */
        Elements[i].val = val;
        Elements[i].rating = r;
    }

    pq->CurrentSize = CurrentSize;

    return;
}

static GCC_INLINE fcs_bool_t fc_solve_is_pqueue_empty(PQUEUE * pq)
{
    return (pq->CurrentSize == 0);
}

/* remove the first node from the pqueue and provide a pointer to it
 *
 * Returns 0 if found.
 * Returns 1 if not found.
 *
 * */

static GCC_INLINE void fc_solve_pq_pop(
    PQUEUE * const pq,
    fcs_collectible_state_t * * const val
)
{
    int i;
    int child;
    pq_element_t * const Elements = pq->Elements;
    typeof(pq->CurrentSize) CurrentSize = pq->CurrentSize;

    if( fc_solve_is_pqueue_empty(pq) )
    {
        *val = NULL;
        return;
    }

    *val = Elements[PQ_FIRST_ENTRY].val;

    /* get pointer to last element in tree */
    const pq_element_t last_elem = Elements[ CurrentSize-- ];

    /* code to pop an element from an ascending (top to bottom) pqueue */

    /*  UNTESTED */

    for( i=PQ_FIRST_ENTRY; (child = PQ_LEFT_CHILD_INDEX(i)) <= CurrentSize; i=child )
    {
        /* set child to the smaller of the two children... */

        if( (child != CurrentSize) &&
            (fcs_pq_rating(Elements[child + 1]) > fcs_pq_rating(Elements[child])) )
        {
            child ++;
        }

        if( fcs_pq_rating( last_elem ) < fcs_pq_rating( Elements[ child ] ) )
        {
            Elements[ i ] = Elements[ child ];
        }
        else
        {
            break;
        }
    }

    Elements[i] = last_elem;
    pq->CurrentSize = CurrentSize;


    return;
}


#ifdef __cplusplus
}
#endif

#endif /* #ifdef FC_SOLVE__PQUEUE_H */
