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

/** @file dacavtypes.h
 *
 * Data types.
 */

#ifndef __defined_dacav_types_h
#define __defined_dacav_types_h

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup DacavData */
/*@{*/

/** Memory free callback function.
 *
 * This type describes the callback parameter of destructors.
 * For instance, if you are using allocated 'char *' as key for hash
 * tables, you may use simply the free(3) as key destroyer. Note that
 * free(3) matches the dfree_cb_t data type.
 *
 * @param[in] obj The object to be freed.
 */
typedef void (*dfree_cb_t) (void *obj);

/** Container scan callback function
 *
 * This type describes the callback parameter of 'foreach' functions. The
 * pattern requires that a container gets scanned, and such a callback
 * gets called for each contained element. The return value of the
 * callback replaces the stored one.
 *
 * A 'foreach' primitive generally requires a udata parameter which is a
 * pointer to generic user defined data. This allows to write reentrant
 * code.
 *
 * @param[in] udata A pointer to some generic user defined data;
 * @param[in] data A pointer to the stored data being considered;
 * @return The new value for the given element.
 */
typedef void * (*diter_cb_t) (void *udata, void *data);

/** Constructor callback function
 *
 * A type of function to be used for creating new instances of a certain
 * structure. The parameter is a user context.
 *
 * @param[in] ctx The user context.
 *
 * @return The newly allocated object.
 */
typedef void * (*dcreate_cb_t) (void *src);

/** Copy-constructor callback function
 *
 * A type of function to be used for copying stored values. Any
 * implementing function is supposed to return a copy of the parameter
 * object.
 *
 * @param[in] src The object to be copied.
 *
 * @return A copy of the input object.
 */
typedef void * (*dcopy_cb_t) (const void *src);

/** Filter scan callback function.
 *
 * This type describes the callback parameter of 'filter' functions. The
 * pattern requires that a container gets scanned, and such a callback
 * gets called for each contained element. The return value of the
 * callback decides if the element should be keeped or removed.
 *
 * A 'filter' primitive generally requires a udata parameter which is a
 * pointer to generic user defined data. This allows to write reentrant
 * code.
 *
 * @param[in] udata A pointer to some generic user defined data;
 * @param[in] data A pointer to the stored data being considered;
 * @return 1 if the element must be keeped, 0 otherwise.
 */
typedef int (dfilter_cb_t) (void *udata, void *data);

/** Callback for comparation between two generic objects.
 *
 * @param[in] v0 The first object to be compared;
 * @param[in] v1 The second object to be compared;
 * @return an integer less than, equal to or greater than zero if v0 is
 *         found, respectively, to be less than, to match, or to be
 *         greater than v1.
 */
typedef int (*dcmp_cb_t) (const void *v0, const void *v1);

/** Pair of callbacks for copy construction and for distruction of a
 * generic item.
 *
 * This is not an opaque type. You can declare an instance of this
 * structure in the stack and pass it by pointer to the
 * dhash_new() function. You can use the same structure multiple
 * time, since the hash table implementation shall store the pointers to
 * the callbacks internally.
 */
typedef struct {
    dfree_cb_t rm;                  /**< Destructor */
    dcopy_cb_t cp;                  /**< Copy-constructor */
} dcprm_t;

/*@}*/

#ifdef __cplusplus
}
#endif

#endif // __defined_dacav_types_h
