/*
 * Copyright 2009 2010 Giovanni Simoni
 *
 * This file is part of LibDacav.
 *
 * LibDacav is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LibDacav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LibDacav.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "dacav.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include <stdio.h>

static
int default_comparer(const void *v0, const void *v1)
{
    uintptr_t diff = (uintptr_t) v0 - (uintptr_t) v1;
    return (int) diff;
}

struct pair {
    void *key;
    void *value;
};

struct dhash {
    unsigned nbuckets;
    dlist_t **buckets;
    struct funcs {
        dhash_cb_t keyhash;     // key hash
        dcmp_cb_t keycmp;       // key comparison

        // Constructor-destructors (may be NULL)
        struct cprm {
            dcprm_t key;  // for key
            dcprm_t val;  // for value
        } cprm;
    } funcs;
};

static
void free_if_needed (void *src, const dcprm_t *cprm)
{
    if (cprm->rm != NULL) {
        cprm->rm(src);
    }
}

static
void free_pair (dhash_t *htab, dhash_pair_t * pair)
{
    struct cprm *cprm = &htab->funcs.cprm;

    free_if_needed(pair->key, &cprm->key);
    free_if_needed(pair->value, &cprm->val);
    free(pair);
}

static
void * copy_if_needed (void *src, const dcprm_t *cprm)
{
    void * dst;

    if (cprm->cp == NULL) {
        dst = src;
    } else {
        dst = cprm->cp(src);
    }
    return dst;
}

static inline
dhash_pair_t * create_pair (dhash_t *htab, const void *key,
                            const void * val)
{
    dhash_pair_t *ret = malloc(sizeof(dhash_pair_t));
    if (ret == NULL) abort();

    /* The double casting is neede to suppress warnings from -Wcast-qual
     * for a situation in which the cast is safe as it is */
    ret->key = copy_if_needed((void *)(intptr_t)key,
                              &htab->funcs.cprm.key);
    ret->value = copy_if_needed((void *)(intptr_t)val,
                                &htab->funcs.cprm.val);

    return ret;
}

static
uintptr_t default_hash(const void *v0)
{
    return (uintptr_t) v0;
}

static
void cprm_init (dcprm_t *dst, const dcprm_t *src)
{
    if (src == NULL) {
        memset((void *) dst, 0, sizeof(dcprm_t));
    } else {
        assert(src->rm != NULL && src->cp != NULL);
        memcpy((void *) dst, (const void *)src, sizeof(dcprm_t));
    }
}

static inline
unsigned get_bucketpos (dhash_t *htab, const void *key)
{
    return htab->funcs.keyhash(key) % htab->nbuckets;
}

static
dhash_result_t locate_element (dhash_t *htab, const void *key, void **found,
                               int remove, dcreate_cb_t create, void *ctx)
{
    dlist_t **bkt = htab->buckets + get_bucketpos(htab, key);
    dhash_pair_t *cursor;

    diter_t *iter = dlist_iter_new(bkt, NULL);
    while (diter_hasnext(iter)) {
        cursor = diter_next(iter);
        if (htab->funcs.keycmp(cursor->key, key) == 0) {
            if (found != NULL) {
                *found = cursor->value;
            }
            if (remove) {
                free_pair(htab, cursor);
                diter_remove(iter);
            }
            dlist_iter_free(iter);
            return DHASH_FOUND;
        }
    }
    dlist_iter_free(iter);

    if (create) {
        dhash_pair_t *new_pair = malloc(sizeof(dhash_pair_t));
        if (new_pair == NULL) abort();

        /* Double casting: avoid -Wcast-qual */
        new_pair->key = copy_if_needed((void *)(intptr_t)key,
                                       &htab->funcs.cprm.key);
        new_pair->value = create(ctx);
        *bkt = dlist_append(*bkt, new_pair);
        if (found != NULL) {
            *found = new_pair->value;
        }
    }

    return DHASH_NOTFOUND;
}

dhash_t *dhash_new (unsigned nbuckets, dhash_cb_t hf, dcmp_cb_t cmp,
                    const dcprm_t *key_cprm, const dcprm_t *val_cprm)
{
    size_t total;
    void *chunk;
    dhash_t *table;
    dlist_t **bks;
    struct funcs * funcs;
    int i;

    assert(nbuckets > 0);

    total = sizeof(struct dhash) + ( nbuckets * sizeof(dlist_t *) );
    chunk = malloc(total);
    if (chunk == NULL) abort();
    table = (dhash_t *) chunk;
    table->nbuckets = nbuckets;
    table->buckets = bks = (dlist_t **)
                           (((char *)chunk) + sizeof(struct dhash));

    funcs = &table->funcs;
    funcs->keyhash = hf == NULL ? default_hash : hf;
    funcs->keycmp = cmp == NULL ? default_comparer : cmp;

    cprm_init(&funcs->cprm.key, key_cprm);
    cprm_init(&funcs->cprm.val, val_cprm);

    for (i = 0; i < nbuckets; i++) {
        bks[i] = dlist_new();
    }

    return table;
}

void dhash_free (dhash_t *htab)
{
    int i;

    for (i = 0; i < htab->nbuckets; i ++) {
        dlist_t *l = htab->buckets[i];
        while (!dlist_empty(l)) {
            dhash_pair_t *p;

            l = dlist_pop(l, (void **)&p);
            free_pair(htab, p);
        }
        dlist_free(l, NULL);
    }
    /* Note: no need of freeing the buckets: they are in the same memory
     * chunk of the hash table itself.
     */
    free(htab);
}

dhash_result_t dhash_insert (dhash_t *htab, const void *key,
                             const void *value)
{
    dlist_t **bkt = htab->buckets + get_bucketpos(htab, key);
    dhash_pair_t *cursor;

    // First of all, do we need a simple replacement?
    diter_t *iter = dlist_iter_new(bkt, NULL);
    while (diter_hasnext(iter)) {
        cursor = diter_next(iter);
        if (htab->funcs.keycmp(cursor->key, key) == 0) {
            // Note: we don't need to use diter_replace(), since the
            // cursor remains the same. Just the value is updated.
            free_if_needed(cursor->value, &htab->funcs.cprm.val);
            cursor->value = copy_if_needed((void *)(intptr_t)value,
                                           &htab->funcs.cprm.val);
            dlist_iter_free(iter);
            return DHASH_FOUND;
        }
    }
    dlist_iter_free(iter);

    // Allocation of a new element
    cursor = create_pair(htab, key, value);
    *bkt = dlist_append(*bkt, (void *)cursor);
    return DHASH_NOTFOUND;
}

dhash_result_t dhash_search_default (dhash_t *htab, const void *key,
                                     void **found, dcreate_cb_t create,
                                     void *ctx)
{
    return locate_element(htab, key, found, 0, create, ctx);
}

dhash_result_t dhash_search (dhash_t *htab, const void *key, void **found)
{
    return locate_element(htab, key, found, 0, NULL, NULL);
}

dhash_result_t dhash_delete (dhash_t *htab, const void *key, void **found)
{
    return locate_element(htab, key, found, 1, NULL, NULL);
}

const void *dhash_key (dhash_pair_t *p)
{
    return p->key;
}

void *dhash_val (dhash_pair_t *p)
{
    return p->value;
}

/* -------------------------------------------------------------------- */

/* Logic for iterators */

struct iterable {
    dhash_t *self;              // A pointer to the hash table itself
    unsigned buck;              // Bucket till now
    diter_t *buck_it;           // Iterator over the current list
    dhash_pair_t * next_yield;  // Last seen from outside pair.
};

static
int iter_hasnext (struct iterable *it)
{
    unsigned buck, nbucks;
    dlist_t **buckets;
    diter_t *buck_it = it->buck_it;

    if (diter_hasnext(buck_it)) {
        /* The current bucket has more elements. */

        /* Retrieve it (this must be done here because we don't know if
         * the user will call iter_remove before iter_next! This shall be
         * actually yielded with iter_next */
        it->next_yield = diter_next(buck_it);
        return 1;
    }

    /* Remove the current iterator which is empty. */
    dlist_iter_free(buck_it);

    buck = it->buck;
    nbucks = it->self->nbuckets;
    buckets = it->self->buckets;

    /* Skip all empty buckets. The prefix ++ is because we already know
     * there's nothing  left in the previous iterator */
    while (++buck < nbucks && dlist_empty(buckets[buck]));

    if (buck < nbucks) {
        /* We stopped because the buck is non-empty (second condition of
         * the previous `while` cycle) */
        it->buck = buck;    // Set new bucket position.

        /* Prepare next iterator */
        it->buck_it = dlist_iter_new(buckets + buck, NULL);
        diter_hasnext(it->buck_it);

        /* Retrieve it (this must be done here because we don't know if
         * the user will call iter_remove before iter_next! */
        it->next_yield = diter_next(it->buck_it);
        return 1;
    }
    it->buck_it = NULL;

    return 0;
}

static
void *iter_next (struct iterable *it)
{
    return it->next_yield;
}

static
void iter_remove (struct iterable *it)
{
    dhash_pair_t * lsp = it->next_yield;
    if (lsp == NULL) abort();

    /* Manually free the pair */
    free_pair(it->self, lsp);
    it->next_yield = NULL;

    /* Remove the pair from the bucket */
    diter_remove(it->buck_it);
}

diter_t *dhash_iter_new (dhash_t *t)
{
    diter_t *ret = diter_new((dnext_t)iter_next,
                             (dhasnext_t)iter_hasnext,
                             (dremove_t)iter_remove,
                             NULL, // No replace function for the moment.
                             sizeof(struct iterable));
    struct iterable *it = diter_get_iterable(ret);
    it->self = t;
    it->buck = 0;

    // Iterator over the first list.
    it->buck_it = dlist_iter_new(t->buckets, NULL);
    it->next_yield = NULL;

    return ret;
}

void dhash_iter_free (diter_t *i)
{
    struct iterable *it = diter_get_iterable(i);
    if (it->buck_it != NULL) {
        dlist_iter_free(it->buck_it);
    }
    diter_free(i);
}

