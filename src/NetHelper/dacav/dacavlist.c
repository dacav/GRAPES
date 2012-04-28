/*
 * Copyright 2009 2012 Giovanni Simoni
 *
 * This file is part of LibDacav. See the README.txt file for license
 * statements of this part
 */

#include "dacav.h"

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

static
int default_comparer(const void *v0, const void *v1)
{
    uintptr_t diff = (uintptr_t) v0 - (uintptr_t) v1;
    return (int) diff;
}

static
void copy_cprm (dcprm_t *dst, const dcprm_t *src)
{
    if (src == NULL || src->cp == NULL) {
        assert(src == NULL || src->rm == NULL);
        dst->cp = NULL;
        dst->rm = NULL;
    } else {
        assert(src->rm != NULL);
        dst->cp = src->cp;
        dst->rm = src->rm;
    }
}


struct dlist {
    void *object;
    dlist_t *next;
    dlist_t *prev;
};

dlist_t *dlist_new ()
{
    return NULL;
}

/* Append the new list node in tail */
static
dlist_t *join_elem (dlist_t *l, dlist_t *n)
{
    if (l == NULL) {
        n->next = n->prev = n;
        return n;
    } else {
        n->next = l;
        n->prev = l->prev;
        l->prev->next = n;
        l->prev = n;
        return l;
    }
}

static
dlist_t *new_elem (void *o)
{
    dlist_t *n;

    n = malloc(sizeof(dlist_t));
    if (n == NULL) abort();
    n->object = o;
    return n;
}

static
void * nocopy (const void * src)
{
    return (void *)(intptr_t) src;
}

dlist_t *dlist_append (dlist_t *l, void *o)
{
    return join_elem(l, new_elem(o));
}

dlist_t *dlist_push (dlist_t *l, void *o)
{
    dlist_t *n = new_elem(o);
    join_elem(l, n);
    return n;
}

dlist_t *dlist_copy (dlist_t *l, dcopy_cb_t cp)
{
    dlist_t *ret;
    dlist_t *stop;

    stop = l;
    ret = dlist_new();
    cp = cp ? cp : nocopy;
    while (l) {
        ret = join_elem(ret, new_elem(cp(l->object)));
        l = l->next;
        if (l == stop) {
            return ret;
        }
    }

    return ret;
}

dlist_t *dlist_pop (dlist_t *l, void **o)
{
    dlist_t *tmp;

    if (l == NULL) {
        *o = NULL;
        return NULL;
    }
    if (l->next == l) {
        *o = l->object;
        free(l);
        return NULL;
    }
    tmp = l;
    l = l->next;
    tmp->prev->next = tmp->next;
    tmp->next->prev = tmp->prev;
    *o = tmp->object;
    free(tmp);
    return l;
}

int dlist_empty (dlist_t *l)
{
    return l == NULL;
}

dlist_t *dlist_foreach (dlist_t *l, diter_cb_t f, void *ud)
{
    dlist_t *cur;

    if (l == NULL) return NULL;
    cur = l;
    do {
        cur->object = f(ud, cur->object);
        cur = cur->next;
    } while (cur != l);
    return l;
}

dlist_t *dlist_filter (dlist_t *l, dfilter_cb_t f, void *ud)
{
    dlist_t *ret, *cur, *tmp;

    if (l == NULL) return NULL;
    l->prev->next = ret = NULL;
    cur = l;
    do {
        tmp = cur;
        cur = cur->next;
        if (f(ud, tmp->object)) {
            ret = join_elem(ret, tmp);
        } else {
            free(tmp);
        }
    } while (cur);

    return ret;
}

// this frees an element of the list. Note: it must be already unlinked
// from both successor and predecessor.
static inline
void free_position (dlist_t *e, dfree_cb_t f)
{
    if (f != NULL) {
        f(e->object);
    }
    free(e);
}

void dlist_free (dlist_t *l, dfree_cb_t f)
{
    dlist_t *e;

    if (l == NULL) return;
    l->prev->next = NULL;
    while (l) {
        e = l;
        l = l->next;
        free_position(e, f);
    }
}

dlist_t *dlist_slice (dlist_t *l, unsigned from, unsigned to,
                      dcopy_cb_t cp)
{
    dlist_t *ret;
    dlist_t *stop = l;

    if (from >= to) {
        return NULL;
    }

    ret = NULL;
    to -= from;
    while (from --) {
        l = l->next;
    }
    cp = cp ? cp : nocopy;
    while (to--) {
        ret = dlist_append(ret, cp(l->object));
        l = l->next;
        if (l == stop) return ret;
    }
    return ret;
}

/* Bubblesort on items content: instead of moving links between objects,
 * why don't we just move stored object pointers? :) */
dlist_t *dlist_sort (dlist_t *l, dcmp_cb_t cmp)
{
    dlist_t *last, *next, *cur;

    if (l == NULL) {
        return NULL;
    }

    if (cmp == NULL) {
        cmp = default_comparer;
    }

    last = l;
    while (last != l->next) {
        cur = l;
        while ((next = cur->next) != last) {
            void * tmp = cur->object;
            if (cmp(tmp, next->object) > 0) {
                /* Need to swap */
                cur->object = next->object;
                next->object = tmp;
            }
            cur = next;
        }
        last = last->prev;
    }

    return l;
}

struct iterable {
    dlist_t *first;
    dlist_t *cursor;
    dlist_t **list;

    dcprm_t cprm;
};

static
int iter_hasnext (struct iterable *it)
{
    dlist_t *first = it->first;
    if (first == NULL) {
        // Empty list, no iteration
        return 0;
    }
    if (first->next == first || first->prev == it->cursor) {
        // Only one element (or only one element left). The next call will
        // return 0
        it->first = NULL;
    }
    // If we have more than one element we run until we reach the first
    // element again.
    return 1;
}

static
void *iter_next (struct iterable *it)
{
    void * ret;
    dlist_t *cur = it->cursor;

    if (cur == NULL)
        return NULL;
    // retrieve the current object
    ret = cur->object;
    // advance to next element
    it->cursor = cur->next;
    return ret;
}

static
void iter_remove (struct iterable *it)
{
    dlist_t *tmp, *cur = it->cursor;
    if (it->first == NULL) {
        // The list may be empty because of iter_hasnext: if we still have
        // a cursor the list must be freed
        if (cur) {
            free_position(cur, it->cprm.rm);
            *(it->list) = NULL;
        }
        it->cursor = NULL;
    } else {
        tmp = cur;
        cur = cur->prev;
        if (cur == it->first) {
            it->first = tmp;
            *(it->list) = tmp;
        }
        cur->prev->next = tmp;
        cur->next->prev = cur->prev;
        free_position(cur, it->cprm.rm);
    }
}

static
void iter_replace (struct iterable *it, void *new_val)
{
    dlist_t *prev;

    prev = it->cursor->prev;
    if (it->cprm.cp) {
        new_val = it->cprm.cp(new_val);
        it->cprm.rm(prev->object);
    }
    prev->object = new_val;
}

diter_t *dlist_iter_new (dlist_t **l, dcprm_t *cprm)
{
    diter_t *ret = diter_new((dnext_t)iter_next, (dhasnext_t)iter_hasnext,
                             (dremove_t)iter_remove,
                             (dreplace_t)iter_replace,
                             sizeof(struct iterable));
    struct iterable *it = diter_get_iterable(ret);
    it->cursor = it->first = *l;
    it->list = l;
    copy_cprm(&it->cprm, cprm);

    return ret;
}

void dlist_iter_free (diter_t *i)
{
    diter_free(i);
}

