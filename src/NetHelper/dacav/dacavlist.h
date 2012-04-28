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

/** @file dacavlist.h
 *
 * Double linked list implementation. 
 */

#ifndef __defined_dacav_dacavlist_h
#define __defined_dacav_dacavlist_h

#include "dacav.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup DacavList */
/*@{*/

/** Opaque type for list */
typedef struct dlist dlist_t;

/** Constructor that allocates a new (empty) list.
 *
 * @return The new list.
 */
dlist_t *dlist_new ();

/** Constructor that allocates a list basing on slicing.
 *
 * This function returns a newly allocated list, independent from the one
 * passed as first parameter, that must be freed with dlist_free, however
 * contained elements are exactly the ones that have been selected from l.
 *
 * In other words this function achieves a shallow copy of part of the
 * original list.
 *
 * @param[in] l The list to be sliced;
 * @param[in] from The index of the first element to be taken;
 * @param[in] to The index of the first element to be excluded;
 * @param[in] cp Copy-constructor callback to be used for copying items.
 *               Provide NULL in order to copy values directly.
 *
 * @return The newly allocated list.
 */
dlist_t *dlist_slice (dlist_t *l, unsigned from, unsigned to,
                      dcopy_cb_t cp);

/** Constructor that makes a shallow copy of the list.
 *
 * @param[in] l The list to be copied;
 * @param[in] cp The copy-constructor callback to be used for copying
 *               items. Provide NULL in order to copy values directly.
 *
 * @return The new list.
 */
dlist_t *dlist_copy (dlist_t *l, dcopy_cb_t cp);

/** Sort the list.
 *
 * Given a list, sort it with the order given by the cmp callback.
 *
 * @param[in] l The list to be sorted;
 * @param[in] cmp The comparsion function to be used for sorting;
 * @return The new head of the list.
 */
dlist_t *dlist_sort (dlist_t *l, dcmp_cb_t cmp);

/** Free the given list
 *
 * @param[in] l The list to be deallocated;
 * @param[in] f The callback used to deallocate stored values (you may
 *              provide NULL if the elements must not be freed).
 */
void dlist_free (dlist_t *l, dfree_cb_t f);

/** Append an object.
 *
 * The object is inserted in the last position of the list. This
 * operation has cost O(1).
 *
 * @param[in] l The list on which the new object must be appended;
 * @param[in] o The object to append;
 * @return The new head of the list.
 */
dlist_t *dlist_append (dlist_t *l, void *o);

/** Push an object.
 *
 * The object is inserted in the first position of the list. This
 * operation has cost O(1).
 *
 * @param[in] l The list on which the new object must be pushed;
 * @param[in] o The object to pushed;
 * @return The new head of the list.
 */
dlist_t *dlist_push (dlist_t *l, void *o);

/** Pop an object.
 *
 * The object stored in the first position gets removed from the list and
 * the o pointer gets updated. If the list is empty the o pointer is set
 * to null.
 *
 * @param[in] l The list from which the object will be removed;
 * @param[in] o A pointer to the location in which the removed object
 *              address will be stored;
 * @return The new head of the list.
 */
dlist_t *dlist_pop (dlist_t *l, void **o);

/** Check if the list is empty.
 *
 * @param[in] l The list to be checked;
 * @return 1 if the list is empty, 0 otherwise.
 */
int dlist_empty (dlist_t *l);

/** Run a callback on each element of the list.
 *
 * @param l The list on which iterate;
 * @param f The function to be applied to each object stored in the list;
 * @param ud Some generic user data;
 * @return The new head of the list.
 *
 * @see diter_cb_t callback type;
 *
 * You may feel not comfortable with this approach for object iteration.
 * If so you may prefer iterators:
 * @see diter_t data type;
 * @see dlist_iter_new function;
 * @see dlist_iter_free function.
 */
dlist_t *dlist_foreach (dlist_t *l, diter_cb_t f, void *ud);

/** Filter elements of the list.
 *
 * This function allows to selectively remove elements from the list by
 * iterating on each element as dlist_foreach does.
 *
 * @param[in] l The list on which iterate;
 * @param[in] f The function to be called on each object stored in the
 *              list;
 * @param[in] ud Some generic user data;
 * @return The new head of the list.
 *
 * @see dfilter_cb_t for further information.
 *
 * You may feel not comfortable with this approach for object iteration.
 * If so you may prefer iterators:
 * @see diter_t data type;
 * @see dlist_iter_new function;
 * @see dlist_iter_free function.
 */
dlist_t *dlist_filter (dlist_t *l, dfilter_cb_t f, void *ud);

/** Build an iterator for the list.
 *
 * @warning As you may have noticed, this function requires as parameter a
 *          pointer pointer. This inherently means that the list you'll be
 *          passed may be modified by the iterator. Be aware of this and
 *          don't mess with the list while iterating on it!
 *
 * @param[in] l A pointer to the list on which the iterator must be built;
 * @param[in] cprm An optional copy/remove semantics for the objects
 *            stored in the list. The destructor is used for
 *            diter_remove(), while diter_replace() uses both
 *            the copy-constructor and the destructor.
 *
 * @return The iterator.
 *
 * @see dcprm_t;
 * @see diter_t for further information.
 */
diter_t *dlist_iter_new (dlist_t **l, dcprm_t *cprm);

/** Destroy the iterator
 *
 * @param[in] i The iterator to destroy.
 * @see diter_t for further information.
 */
void dlist_iter_free (diter_t *i);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif // __defined_dacav_dacavlist_h
