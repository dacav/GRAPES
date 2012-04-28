/*
 * Copyright 2009 2012 Giovanni Simoni
 *
 * This file is part of LibDacav. See the README.txt file for license
 * statements of this part
 */

#include "dacav.h"

#include <stdlib.h>
#include <assert.h>

struct diter {
    void *iterable;
    dnext_t next;
    dhasnext_t hasnext;
    dremove_t remove;
    dreplace_t replace;
};

void * diter_next(diter_t *i)
{
    return i->next(i->iterable);
}

int diter_hasnext (diter_t *i)
{
    return i->hasnext(i->iterable);
}

void diter_remove (diter_t *i)
{
    // Optional
    dremove_t remove = i->remove;
    if (remove) {
        remove(i->iterable);
    }
}

void diter_replace (diter_t *i, void *new_val)
{
    // Optional
    dreplace_t replace = i->replace;
    if (replace) {
        replace(i->iterable, new_val);
    }
}

void *diter_get_iterable (diter_t *i)
{
    return i->iterable;
}

diter_t *diter_new (dnext_t nx, dhasnext_t hnx, dremove_t rm,
                    dreplace_t rep, unsigned iterable_size)
{
    diter_t *ret;

    ret = malloc(sizeof(struct diter));
    if (ret == NULL) abort();

    ret->iterable = malloc(iterable_size);
    assert(ret->iterable != NULL);

    ret->next = nx;
    ret->hasnext = hnx;
    ret->remove = rm;
    ret->replace = rep;

    return ret;
}

void diter_free (diter_t *i)
{
    free(i->iterable);
    free(i);
}
