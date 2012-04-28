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
