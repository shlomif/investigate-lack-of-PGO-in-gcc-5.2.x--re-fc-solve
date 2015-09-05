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
 * offloading_queue.h - header file for the offloading-to-hard-disk
 * queue.
 */

#ifndef FC_SOLVE__OFFLOADING_QUEUE_H
#define FC_SOLVE__OFFLOADING_QUEUE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "state.h"
#include "inline.h"
#include "bool.h"

typedef const unsigned char * fcs_offloading_queue_item_t;
#if !defined(FCS_DBM_USE_OFFLOADING_QUEUE)

typedef struct fcs_Q_item_wrapper_struct
{
    fcs_offloading_queue_item_t datum;
    struct fcs_Q_item_wrapper_struct * next;
} fcs_Q_item_wrapper_t;

typedef struct
{
    fcs_compact_allocator_t queue_allocator;
    fcs_Q_item_wrapper_t * queue_head, * queue_tail, * queue_recycle_bin;
    long num_inserted, num_items_in_queue, num_extracted;
} fcs_offloading_queue_t;

static GCC_INLINE void fcs_offloading_queue__init(
    fcs_offloading_queue_t * queue,
    fcs_meta_compact_allocator_t * meta_alloc
    )
{
    fc_solve_compact_allocator_init(
        &(queue->queue_allocator), meta_alloc
    );

    queue->queue_head = queue->queue_tail = queue->queue_recycle_bin = NULL;
    queue->num_inserted = queue->num_items_in_queue = queue->num_extracted = 0;

    return;
}

static GCC_INLINE void fcs_offloading_queue__destroy(
    fcs_offloading_queue_t * queue
    )
{
    fc_solve_compact_allocator_finish(&(queue->queue_allocator));
}

static GCC_INLINE fcs_bool_t fcs_offloading_queue__extract(
    fcs_offloading_queue_t * queue,
    fcs_offloading_queue_item_t * return_item
)
{
    fcs_Q_item_wrapper_t * item = queue->queue_head;

    if (! item)
    {
        return FALSE;
    }

    *return_item = item->datum;

    if (! ( queue->queue_head = item->next ) )
    {
        queue->queue_tail = NULL;
    }

    item->next = queue->queue_recycle_bin;
    queue->queue_recycle_bin = item;

    queue->num_items_in_queue--;
    queue->num_extracted++;

    return TRUE;
}

static GCC_INLINE void fcs_offloading_queue__insert(
    fcs_offloading_queue_t * queue,
    const fcs_offloading_queue_item_t * datum
)
{
    fcs_Q_item_wrapper_t * new_item;

    if (queue->queue_recycle_bin)
    {
        queue->queue_recycle_bin =
            (new_item = queue->queue_recycle_bin)->next;
    }
    else
    {
        new_item =
            (fcs_Q_item_wrapper_t *)
            fcs_compact_alloc_ptr(
                &(queue->queue_allocator),
                sizeof(*new_item)
                );
    }
    new_item->datum = *datum;
    new_item->next = NULL;

    if (queue->queue_tail)
    {
        queue->queue_tail = queue->queue_tail->next = new_item;
    }
    else
    {
        queue->queue_head = queue->queue_tail = new_item;
    }

    queue->num_inserted++;
    queue->num_items_in_queue++;

    return;
}

/* Implement the standard in-memory queue as a linked list. */
#else


typedef struct
{
    int num_items_per_page;
    long page_index, queue_id;
    int write_to_idx;
    int read_from_idx;
    unsigned char * data;
} fcs_offloading_queue_page_t;

static GCC_INLINE void fcs_offloading_queue_page__recycle(
    fcs_offloading_queue_page_t * const page
    )
{
    page->write_to_idx = 0;
    page->read_from_idx = 0;
}

static GCC_INLINE void fcs_offloading_queue_page__init(
    fcs_offloading_queue_page_t * const page,
    const int num_items_per_page,
    const long page_index,
    const long queue_id
    )
{
    fcs_offloading_queue_page_t new_page =
    {
        .num_items_per_page = num_items_per_page,
        .page_index = page_index,
        .queue_id = queue_id,
        .data = malloc(
            sizeof(fcs_offloading_queue_item_t) * num_items_per_page
        )
    };
    *page = new_page;
    fcs_offloading_queue_page__recycle(page);

    return;
}

static GCC_INLINE void fcs_offloading_queue_page__destroy(
    fcs_offloading_queue_page_t * const page
)
{
    free(page->data);
    page->data = NULL;
}

static GCC_INLINE fcs_bool_t fcs_offloading_queue_page__can_extract(
    const fcs_offloading_queue_page_t * const page
)
{
    return (page->read_from_idx < page->write_to_idx);
}

static GCC_INLINE void fcs_offloading_queue_page__extract(
    fcs_offloading_queue_page_t * const page,
    fcs_offloading_queue_item_t * const out_item
)
{
    memcpy(
        out_item,
        (page->data + sizeof(*out_item) * ((page->read_from_idx)++)),
        sizeof(*out_item)
    );
}

static GCC_INLINE fcs_bool_t fcs_offloading_queue_page__can_insert(
    const fcs_offloading_queue_page_t * const page
)
{
    return (page->write_to_idx < page->num_items_per_page);
}

static GCC_INLINE void fcs_offloading_queue_page__insert(
    fcs_offloading_queue_page_t * const page,
    const fcs_offloading_queue_item_t * const in_item
    )
{
    memcpy(
        page->data + ((page->write_to_idx)++) * sizeof(*in_item),
        in_item,
        sizeof(*in_item)
    );
}

static GCC_INLINE const char * fcs_offloading_queue_page__calc_filename(
    fcs_offloading_queue_page_t * const page,
    char * const buffer,
    const char * const offload_dir_path)
{
    sprintf(buffer, "%s/fcs_queue%lXq_%020lX.page", offload_dir_path, page->queue_id, page->page_index);

    return buffer;
}

static GCC_INLINE void fcs_offloading_queue_page__start_after(
    fcs_offloading_queue_page_t * const page,
    const fcs_offloading_queue_page_t * const other_page
    )
{
    page->page_index = other_page->page_index+1;
    fcs_offloading_queue_page__recycle(page);
}

static GCC_INLINE void fcs_offloading_queue_page__bump(
    fcs_offloading_queue_page_t * const page
)
{
    fcs_offloading_queue_page__start_after(page, page);
}

static GCC_INLINE void fcs_offloading_queue_page__read_next_from_disk(
    fcs_offloading_queue_page_t * const page,
    const char * const offload_dir_path
    )
{
    fcs_offloading_queue_page__bump(page);
    char page_filename[PATH_MAX+1];
    fcs_offloading_queue_page__calc_filename(page, page_filename, offload_dir_path);

    FILE * const f = fopen(page_filename, "rb");
    fread( page->data, sizeof(fcs_offloading_queue_item_t),
           page->num_items_per_page, f
    );
    fclose(f);

    /* We need to set this limit because it's a read-only page that we
     * retrieve from the disk and otherwise ->can_extract() will return
     * false for most items.
     * */
    page->write_to_idx = page->num_items_per_page;

    unlink(page_filename);
}

static GCC_INLINE void fcs_offloading_queue_page__offload(
    fcs_offloading_queue_page_t * const page,
    const char * const offload_dir_path
    )
{
    char page_filename[PATH_MAX+1];

    fcs_offloading_queue_page__calc_filename(page, page_filename, offload_dir_path);

    FILE * const f = fopen(page_filename, "wb");
    fwrite( page->data, sizeof(fcs_offloading_queue_item_t),
           page->num_items_per_page, f
    );
    fclose(f);
}

typedef struct
{
    int num_items_per_page;
    const char * offload_dir_path;
    long num_inserted, num_items_in_queue, num_extracted;
    long id;
    /*
     * page_idx_to_write_to, page_idx_for_backup and page_idx_to_read_from
     * always point to the two "pages" below, but they can be swapped and
     * page_idx_for_backup may be NULL.
     */
    int page_idx_to_write_to, page_idx_for_backup, page_idx_to_read_from;
    fcs_offloading_queue_page_t pages[2];
} fcs_offloading_queue_t;

static GCC_INLINE void fcs_offloading_queue__init(
    fcs_offloading_queue_t * queue,
    int num_items_per_page,
    const char * offload_dir_path,
    long id
    )
{
    queue->num_items_per_page = num_items_per_page;
    queue->offload_dir_path = offload_dir_path;
    queue->num_inserted = queue->num_items_in_queue = queue->num_extracted = 0;
    queue->id = id;

    fcs_offloading_queue_page__init(&(queue->pages[0]), num_items_per_page, 0, queue->id);
    fcs_offloading_queue_page__init(&(queue->pages[1]), num_items_per_page, 0, queue->id);

    queue->page_idx_to_read_from = queue->page_idx_to_write_to = 0;
    queue->page_idx_for_backup = 1;
}

static GCC_INLINE void fcs_offloading_queue__destroy(
    fcs_offloading_queue_t * queue
    )
{
    fcs_offloading_queue_page__destroy(&(queue->pages[0]));
    fcs_offloading_queue_page__destroy(&(queue->pages[1]));
}

static GCC_INLINE void fcs_offloading_queue__insert(
    fcs_offloading_queue_t * queue,
    const fcs_offloading_queue_item_t * item
)
{
    if (! fcs_offloading_queue_page__can_insert(queue->pages + queue->page_idx_to_write_to))
    {
        if (queue->pages[queue->page_idx_to_read_from].page_index != queue->pages[queue->page_idx_to_write_to].page_index)
        {
            fcs_offloading_queue_page__offload(
                queue->pages + queue->page_idx_to_write_to,
                queue->offload_dir_path
            );
            fcs_offloading_queue_page__bump(queue->pages + queue->page_idx_to_write_to);
        }
        else
        {
            queue->page_idx_to_write_to = queue->page_idx_for_backup;
            fcs_offloading_queue_page__start_after(
                queue->pages + queue->page_idx_to_write_to,
                queue->pages + queue->page_idx_to_read_from
            );
            queue->page_idx_for_backup = -1;
        }
    }

    fcs_offloading_queue_page__insert(
        queue->pages + queue->page_idx_to_write_to,
        item
    );

    queue->num_inserted++;
    queue->num_items_in_queue++;
}

static GCC_INLINE fcs_bool_t fcs_offloading_queue__extract(
    fcs_offloading_queue_t * queue,
    fcs_offloading_queue_item_t * return_item
)
{
    if (queue->num_items_in_queue == 0)
    {
        return FALSE;
    }

    if (! fcs_offloading_queue_page__can_extract(queue->pages + queue->page_idx_to_read_from))
    {
        /* Cannot really happen due to the num_items_in_queue check.
         *
         * if (queue->_page_idx_to_read_from->page_index ==
         *     queue->_page_idx_to_write_to->page_index)
        */
        if (queue->pages[queue->page_idx_to_read_from].page_index + 1 == queue->pages[queue->page_idx_to_write_to].page_index)
        {
            queue->page_idx_for_backup = queue->page_idx_to_read_from;
            queue->page_idx_to_read_from = queue->page_idx_to_write_to;
        }
        else
        {
            fcs_offloading_queue_page__read_next_from_disk(
                queue->pages + queue->page_idx_to_read_from,
                queue->offload_dir_path
            );
        }
    }

    queue->num_items_in_queue--;
    queue->num_extracted++;

    fcs_offloading_queue_page__extract(
        queue->pages + queue->page_idx_to_read_from,
        return_item
    );

    return TRUE;
}

#endif

#ifdef __cplusplus
}
#endif

#endif  /* FC_SOLVE__OFFLOADING_QUEUE_H */
