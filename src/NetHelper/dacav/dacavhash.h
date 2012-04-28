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

/** @file dacavhash.h
 *
 * Hash table implementation.
 */

#ifndef __defined_dacav_dacavdhash_h
#define __defined_dacav_dacavdhash_h

#ifdef __cplusplus
extern "C" {
#endif

#include "dacav.h"
#include <stdint.h>

/** @addtogroup DacavHash */
/*@{*/

/** Opaque type for hash table. */
typedef struct dhash dhash_t;

/** Callback for hashing of a generic object. */
typedef uintptr_t (*dhash_cb_t) (const void *key);

/** Opaque structure that contains the <key,value> pair.
 *
 * @see dhash_key is a function for key extraction;
 * @see dhash_val is a function for value extraction;
 * @see DacavHash contains a section with further information about
 *                iterator semantics for hash tables.
 */
typedef struct pair dhash_pair_t;

/** Return values for hash lookup functions.
 *
 * @see dhash_search;
 * @see dhash_delete;
 * @see dhash_insert.
 */
typedef enum {
    DHASH_FOUND = 0,    /**< Object found; */
    DHASH_NOTFOUND = 1, /**< Object not found. */
} dhash_result_t;

/** Given a dhash_pair_t element, returns the corresponding key.
 *
 * This is used to extract the key from pairs yielded by the iterator. The
 * returned key is internal to the table. As such it cannot be modified.
 *
 * @see DacavHash contains a section with further information about
 *                iterator semantics for hash tables.
 *
 * @param[in] p The <key,value> pair.
 *
 * @return The key.
 */
const void *dhash_key (dhash_pair_t *p);

/** Given a dhash_pair_t element, returns the corresponding value.
 *
 * This is used to extract the value from pairs yielded by the iterator.
 * The returned value is internal to the table, and unlike for the key,
 * modifying it is allowed (it will result in changing the stored value).
 *
 * @see DacavHash contains a section with further information about
 *                iterator semantics for hash tables.
 *
 * @param[in] p The <key,value> pair.
 *
 * @return The value.
 */
 void *dhash_val (dhash_pair_t *p);

/** Complete constructor for a hash table.
 *
 * @param[in] nbuckets The number of buckets of the table (should be
 *                     somehow proportional to the number of inserted
 *                     objects). A prime number would be a good choice;
 * @param[in] hf The callback used to retrieve the hash value for the
 *               keys. Provide NULL if the key pointer should be directly
 *               used as hash;
 * @param[in] cmp The callback used to compare two keys, must behave
 *                like the strcmp(3) function. Provide NULL if the key
 *                pointer should be directly used as hash;
 * @param[in] key_cprm A pointer to a copy-constructor and destructor
 *                     pair to be used with keys. If NULL is provided, the
 *                     inserted keys shall be directly stored, and no
 *                     internal copy for keys shall be allocated.
 * @param[in] val_cprm A pointer to a copy-constructor and destructor
 *                     pair to be used with values. If NULL is provided,
 *                     the inserted keys shall be directly stored, and no
 *                     internal copy for values shall be allocated.
 *
 * @see dhash_new_simple;
 * @see dcprm_t.
 *
 * @return A pointer to the allocated hash table.
 */
dhash_t *dhash_new (unsigned nbuckets, dhash_cb_t hf, dcmp_cb_t cmp,
                    const dcprm_t *key_cprm, const dcprm_t *val_cprm);

/** Destructor for a hash table.
 *
 * @param[in] htab The table to destroy;
 */
void dhash_free (dhash_t *htab);

/** Value insertion procedure.
 *
 * Depending on how the constructor has been parametrized, this function
 * may directly store the given pointers for key and value, or may build
 * an internal copy of them. In the latter case the copies will also be
 * freed when calling dhash_free().
 *
 * @param[in] htab The table in which insertion must be achieved;
 * @param[in] key Key for the inserted value;
 * @param[in] value The value to be inserted.
 *
 * @retval dhash_result_t::DHASH_FOUND if there has been a replacement;
 * @retval dhash_result_t::DHASH_NOTFOUND if a new object has been added;
 */
dhash_result_t dhash_insert (dhash_t *htab, const void *key,
                             const void *value);

/** Search for a value by providing a key. If it doesn't exist create it.
 *
 * This function works as dhash_search(), but also allows to specify a
 * callback to be called when the object you are searching cannot be
 * found. The callback shall return an object which will be directly
 * inserted as value.
 *
 * @param[in] htab The table in which insertion must be achieved;
 * @param[in] key Key for the inserted value;
 * @param[out] found The value to be inserted.
 * @param[in] create The callback for object creation;
 * @param[in] ctx The context for the callback.
 *
 * @retval dhash_result_t::DHASH_FOUND if the element has been found;
 * @retval dhash_result_t::DHASH_NOTFOUND if the element has not been
 *         found and the new element has been allocated.
 *
 * @see dcreate_cb_t.
 */
dhash_result_t dhash_search_default (dhash_t *htab, const void *key,
                                     void **found, dcreate_cb_t create,
                                     void *ctx);

/** Search for a value by providing a key.
 *
 * @param[in] htab The hash table to search in;
 * @param[in] key The key to be searched;
 * @param[out] found A pointer to a location in which a pointer to the
 *                   value will be stored. You may pass NULL as found
 *                   parameter in order to simply test the presence of the
 *                   required <key,value> pair.
 *
 * @retval dhash_result_t::DHASH_FOUND on success;
 * @retval dhash_result_t::DHASH_NOTFOUND on lookup failure.
 *
 * @note In case of success *found will point to the internally stored
 *       value, thus modification will result in internal modification. In
 *       case of lookup failure, the *found pointer won't be modified.
 */
dhash_result_t dhash_search (dhash_t *htab, const void *key, void **found);

/** Delete the value associated to the given key.
 *
 * This function behaves exactly as dhash_search() except that, if the
 * object is found, it gets removed.
 *
 * @param[in] htab The hash table to search in;
 * @param[in] key The key to be searched;
 * @param[out] found A pointer to a location in which the value will be
 *                   stored; You may pass NULL as found parameter in order
 *                   to simply delete the entry without having it back.
 *
 * @retval dhash_result_t::DHASH_FOUND on success;
 * @retval dhash_result_t::DHASH_NOTFOUND on lookup failure.
 *
 * @note In case of failure the table remains unchanged.
 */
dhash_result_t dhash_delete (dhash_t *htab, const void *key, void **found);

/** Build an iterator for the hash table.
 *
 * @param[in] t The table on which the iterator must be built.
 *
 * @return The iterator.
 *
 * @see DacavHash contains a section with further information about
 *                iterator semantics for hash tables.
 */
diter_t *dhash_iter_new (dhash_t *t);

/** Destroy the iterator.
 *
 * @param[in] i The iterator to destroy.
 *
 * @see diter_t.
 */
void dhash_iter_free (diter_t *i);

/*@}*/

#ifdef __cplusplus
}
#endif

#endif // __defined_dacav_dhash_h
