/*
 *  Copyright (c) 2012 Arber Fama,
 *                     Giovanni Simoni
 *
 *  This is free software; see lgpl-2.1.txt
 */

#include <dacav/dacav.h>

#include <stdlib.h>
#include <unistd.h>

#include "utils.h"
#include "dictionary.h"
#include "sockaddr-helpers.h"

struct dict {
    dhash_t *hash;
    size_t nelems;
};

static const unsigned DEFAULT_BUCKETS = 17;
static const char CONF_KEY_BUCKETS[] = "TCPHashBuckets";

static void * dict_data_copy (const void * ptr);

dict_t dict_new (struct tag *cfg)
{
    dict_t d;
    int nbuckets;
    dcprm_t cprm_k;
    dcprm_t cprm_v;

    nbuckets = DEFAULT_BUCKETS;
    if (cfg) {
        config_value_int_default(cfg, CONF_KEY_BUCKETS, &nbuckets,
                                 DEFAULT_BUCKETS);
    }

    cprm_k.cp = (dcopy_cb_t) sockaddr_dup;
    cprm_k.rm = free;

    cprm_v.cp = dict_data_copy;
    cprm_v.rm = (dfree_cb_t) client_del;

    d = mem_new(sizeof(struct dict));
    d->hash = dhash_new(nbuckets,
                        (dhash_cb_t) sockaddr_hash,
                        (dcmp_cb_t) sockaddr_cmp,
                        &cprm_k, &cprm_v);
    d->nelems = 0;

    return d;
}

void dict_del (dict_t d)
{
    if (d == NULL) return;
    dhash_free(d->hash);
    free(d);
}

client_t dict_search (dict_t d, const sockaddr_t *addr)
{
    client_t cl;
    dhash_result_t res;

    res = dhash_search_default(d->hash, (const void *)addr, (void **) &cl,
                               (dcreate_cb_t)client_new, (void *)addr);
    if (res == DHASH_NOTFOUND) {
        d->nelems ++;
    } else if (!client_valid(cl)) {
        client_reset(cl);
    }
    return cl;
}

void dict_foreach (dict_t d, dict_foreach_t cb, void * ctx)
{
    diter_t *iter;

    if (d->nelems == 0) return;
    iter = dhash_iter_new(d->hash);
    while (diter_hasnext(iter)) {
        dhash_pair_t *P;
        client_t cl;

        P = diter_next(iter);
        cl = dhash_val(P);
        if (!client_valid(cl)) {
            diter_remove(iter);
            d->nelems --;
            continue;
        }

        if (cb(ctx, dhash_key(P), cl) == 0) {
            break;
        }
    }
    dhash_iter_free(iter);
}

static void * dict_data_copy (const void * ptr)
{
    /* If doing this there's something wrong... */
    abort();
}
