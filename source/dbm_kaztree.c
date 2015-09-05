#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#include "alloc_wrap.h"
#include "dbm_solver.h"
#include "generic_tree.h"

#include "dbm_kaztree_compare.h"

typedef struct
{
    dict_t * kaz_tree;
    fcs_meta_compact_allocator_t meta_alloc;
#ifndef FCS_LIBAVL_STORE_WHOLE_KEYS
    fcs_compact_allocator_t allocator;
#endif
} dbm_t;

void fc_solve_dbm_store_init(fcs_dbm_store_t * store, const char * path, void * * recycle_bin_ptr)
{
    dbm_t * const db = SMALLOC1(db);

    fc_solve_meta_compact_allocator_init(
        &(db->meta_alloc)
    );

    db->kaz_tree =
        fc_solve_kaz_tree_create(compare_records, NULL, &(db->meta_alloc), recycle_bin_ptr);

#ifndef FCS_LIBAVL_STORE_WHOLE_KEYS
    fc_solve_compact_allocator_init(
        &(db->allocator), &(db->meta_alloc)
    );
#endif

    *store = (fcs_dbm_store_t)db;
    return;
}

dict_t * fc_solve_dbm_store_get_dict(
    fcs_dbm_store_t store
    )
{
    return (((dbm_t *)store)->kaz_tree);
}

/*
 * Returns TRUE if the key was added (it didn't already exist.)
 * */
fcs_dbm_record_t * fc_solve_dbm_store_insert_key_value(
    fcs_dbm_store_t store,
    const fcs_encoded_state_buffer_t * key,
    fcs_dbm_record_t * parent,
    const fcs_bool_t should_modify_parent
)
{
#ifdef FCS_LIBAVL_STORE_WHOLE_KEYS
    fcs_dbm_record_t record_on_stack;

    /* This memset() call is done to please valgrind and for general
     * good measure. It is not absolutely necessary (but should not
     * hurt much). It is needed due to struct padding and alignment
     * issues.
     * */
    memset (&record_on_stack, '\0', sizeof(record_on_stack));
#endif

    dbm_t * const db = (dbm_t *)store;

    fcs_dbm_record_t * to_check;
#ifdef FCS_LIBAVL_STORE_WHOLE_KEYS
    to_check = &record_on_stack;
#else
    to_check = (fcs_dbm_record_t *)fcs_compact_alloc_ptr(&(db->allocator), sizeof(*to_check));
#endif

#ifdef FCS_DBM_RECORD_POINTER_REPR
    to_check->key = *key;
    fcs_dbm_record_set_parent_ptr(to_check, parent);
#else
    to_check->key = *key;
    to_check->parent = parent->parent;
#endif
    fcs_bool_t ret =
        (fc_solve_kaz_tree_alloc_insert(db->kaz_tree, to_check) == NULL);

#ifndef FCS_LIBAVL_STORE_WHOLE_KEYS
    if (! ret)
    {
        fcs_compact_alloc_release(&(db->allocator));
    }
#endif
    if (ret)
    {
#ifdef FCS_EXPLICIT_REFCOUNT
        if (should_modify_parent && parent)
        {
            fcs_dbm_record_increment_refcount(parent);
        }
#endif

        return ((fcs_dbm_record_t *)(fc_solve_kaz_tree_lookup_value(db->kaz_tree, to_check)));
    }
    else
    {
        return NULL;
    }
}


fcs_bool_t fc_solve_dbm_store_lookup_parent(
    fcs_dbm_store_t store,
    const unsigned char * key,
    unsigned char * parent
    )
{
    fcs_dbm_record_t to_check;
    to_check.key = *(const fcs_encoded_state_buffer_t *)key;

    dict_key_t existing = fc_solve_kaz_tree_lookup_value(((dbm_t *)store)->kaz_tree, &to_check);

    if (! existing)
    {
        return FALSE;
    }
    else
    {
#ifdef FCS_DBM_RECORD_POINTER_REPR
        fcs_dbm_record_t * const p = fcs_dbm_record_get_parent_ptr((fcs_dbm_record_t *)existing);

        if (p)
        {
            *(fcs_encoded_state_buffer_t *)parent
                = p->key;
        }
        else
        {
            fcs_init_encoded_state((fcs_encoded_state_buffer_t *)parent);
        }
#else
        *(fcs_encoded_state_buffer_t *)parent
            = ((fcs_dbm_record_t *)existing)->parent;
#endif
        return TRUE;
    }
}

extern void fc_solve_dbm_store_destroy(fcs_dbm_store_t store)
{
    dbm_t * const db = (dbm_t *)store;

    fc_solve_kaz_tree_destroy( db->kaz_tree );
#ifndef FCS_LIBAVL_STORE_WHOLE_KEYS
    fc_solve_compact_allocator_finish( &(db->allocator) );
#endif
    fc_solve_meta_compact_allocator_finish( &(db->meta_alloc) );
    free(db);
}
