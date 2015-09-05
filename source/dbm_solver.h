#ifndef FC_SOLVE_DBM_SOLVER_H
#define FC_SOLVE_DBM_SOLVER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "config.h"

#include "bool.h"
#include "inline.h"
#include "generic_tree.h"

#include "dbm_solver_key.h"

/* We need it for the typedef of fcs_fcc_move_t. */
#include "fcc_brfs_test.h"

#ifndef FCS_DBM_WITHOUT_CACHES

#include "dbm_lru_cache.h"

typedef union fcs_pre_cache_key_val_pair_struct
{
    struct {
        fcs_encoded_state_buffer_t key;
        fcs_encoded_state_buffer_t parent;
    };
    union fcs_pre_cache_key_val_pair_struct * next;
} fcs_pre_cache_key_val_pair_t;

typedef struct {
    dict_t * kaz_tree;
    fcs_compact_allocator_t kv_allocator;
    fcs_pre_cache_key_val_pair_t * kv_recycle_bin;
    long count_elements;
} fcs_pre_cache_t;
#endif

typedef void * fcs_dbm_store_t;
void fc_solve_dbm_store_init(fcs_dbm_store_t * store, const char * path, void * * recycle_bin_ptr);

dict_t * fc_solve_dbm_store_get_dict(fcs_dbm_store_t store);

fcs_bool_t fc_solve_dbm_store_does_key_exist(
    fcs_dbm_store_t store,
    const unsigned char * key_raw
);

fcs_bool_t fc_solve_dbm_store_lookup_parent(
    fcs_dbm_store_t store,
    const unsigned char * const key,
    unsigned char * const parent
    );

fcs_dbm_record_t * fc_solve_dbm_store_insert_key_value(
    fcs_dbm_store_t store,
    const fcs_encoded_state_buffer_t * key,
    fcs_dbm_record_t * parent,
    const fcs_bool_t should_modify_parent
    );

#ifndef FCS_DBM_WITHOUT_CACHES
void fc_solve_dbm_store_offload_pre_cache(
    fcs_dbm_store_t store,
    fcs_pre_cache_t * const pre_cache
);
#endif

void fc_solve_dbm_store_destroy(fcs_dbm_store_t store);

typedef struct fcs_dbm_queue_item_struct
{
    fcs_encoded_state_buffer_t key;
    /* TODO : maybe get rid of moves_seq with FCS_DBM_WITHOUT_CACHES
     * to save space. */
    fcs_fcc_moves_seq_t moves_seq;
#ifdef FCS_DBM_CACHE_ONLY
    fcs_fcc_move_t * moves_to_key;
#endif

    struct fcs_dbm_queue_item_struct * next;
} fcs_dbm_queue_item_t;

enum TERMINATE_REASON
{
    DONT_TERMINATE = 0,
    QUEUE_TERMINATE,
    MAX_ITERS_TERMINATE,
    SOLUTION_FOUND_TERMINATE
};

#define MAX_FCC_DEPTH (RANK_KING * 4 * DECKS_NUM * 2)

#ifdef __cplusplus
}
#endif

#endif /*  FC_SOLVE_DBM_SOLVER_H */
