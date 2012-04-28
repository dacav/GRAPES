/*
 * Copyright 2009 2012 Giovanni Simoni
 *
 * This file is part of LibDacav. See the README.txt file for license
 * statements of this part
 */

/** @file dacaviter.h
 *
 * Iterator implementation. 
 */

#ifndef __defined_dacav_dacaviter_h
#define __defined_dacav_dacaviter_h

#include "dacav.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup DacavIter */
/*@{*/

/** Opaque type for iterators.
 *
 * Constructors and destructors for this type are strictly dependent from
 * the kind of iterator used. Every type of container implements them and
 * gives them an appropriate name.
 *
 * @see dhash_iter_new is an example of iterator constructor;
 * @see dhash_iter_free is an example of iterator destructor.
 */
typedef struct diter diter_t;

/** Has-Next predicate, checks if the iterator has at least another element.
 *
 * @warning Iterator semantics requires this function to be called after
 *          and before every use of diter_next(). A wrong usage with
 *          respect to iterator semantics may lead to an inconsistent
 *          state depending on the specific implementation.
 *
 * @param[in] i The iterator.
 *
 * @retval 1 if there's at least another element to iterate on;
 * @retval 0 otherwise.
 */
int diter_hasnext (diter_t *i);

/** Get-Next function, extracts the next element of the iteration.
 *
 * @warning Iterator semantics requires this function to be called after
 *          checking the presence of the element thorugh the diter_hasnext()
 *          predicate. A wrong usage with respect to iterator semantics
 *          may lead to an inconsistent state depending on the specific
 *          implementation.
 *
 * @param[in] i The iterator.
 * @return The next element, which must exist.
 */
void * diter_next(diter_t *i);

/** Remove function, removes from the structure the current element.
 *
 * The removed element is deallocated according to a user-provided
 * semantics which is part of the container.
 *
 * @see dcprm_t.
 *
 * @warning Iterator semantics requires this function to be called only
 *          after checking the presence of the element thorugh the
 *          diter_hasnext() predicate. Calling diter_next() before
 *          diter_remove() doesn't affect the behaviour. A wrong usage
 *          with respect to iterator semantics may lead to an inconsistent
 *          state depending on the specific implementation.
 * 
 * @note Implementation of a remove function is not mandatory. If the data
 *       structure which provides the iterator is not implementing it,
 *       calling diter_remove() has no effect. Please refer to the
 *       documentation of the container you are iterating on.
 *
 * @param[in] i The iterator.
 */
void diter_remove (diter_t *i);

/** Replace function, replaces the current element of the structure.
 *
 * @warning Iterator semantics requires this function to be called only
 *          after checking the presence of the element thorugh the
 *          diter_hasnext() predicate. Calling diter_next() before
 *          diter_remove() doesn't affect the behaviour. A wrong usage
 *          with respect to iterator semantics may lead to an inconsistent
 *          state depending on the specific implementation.
 *
 * @note Implementation of a remove function is not mandatory. If the data
 *       structure which provides the iterator is not implementing it,
 *       calling diter_remove() has no effect.
 *
 * @param[in] i The iterator;
 * @param[in] n The new element that will replace the next element.
 */
void diter_replace (diter_t *i, void *n);

/*@}*/

/** @addtogroup InternalIter */
/*@{*/

/** Type defining the Has-Next function for iterators.
 *
 * This type is used by the container internals, that must provide such a
 * function to the iterator building procedure.
 *
 * @param[in] iterable This parameter strictly depends on the interator
 *                     implementation. It typically contains some data
 *                     used by the iterating process.
 * @retval 1 if the iterator has more elements;
 * @retval 0 if the iterator has no more elements.
 *
 * @see InternalIter.
 */
typedef int (*dhasnext_t) (void *iterable);

/** Type defining the Next function for iterators.
 *
 * @note This type is used by the container internals, that must provide
 *       such a function to the iterator building procedure. If you are
 *       just using the library, you probably will never care about this
 *       data type.
 *
 * @param[in] iterable This parameter strictly depends on the interator
 *                     implementation. It typically contains some data
 *                     used by the iterating process.
 * @return The next value of the iterator.
 *
 * @see InternalIter.
 */
typedef void * (*dnext_t) (void *iterable);

/** Type defining the Remove function for iterators.
 *
 * @note This type is used by the container internals, that must provide
 *       such a function to the iterator building procedure. If you are
 *       just using the library, you probably will never care about this
 *       data type.
 *
 * @param[in] iterable This parameter strictly depends on the interator
 *                     implementation. It typically contains some data
 *                     used by the iterating process;
 *
 * @see InternalIter.
 */
typedef void (*dremove_t) (void *iterable);

/** Type defining the Replace function for iterators.
 * 
 * @note This type is used by the container internals, that must provide
 *       such a function to the iterator building procedure. If you are
 *       just using the library, you probably will never care about this
 *       data type.
 *
 * @param[in] iterable This parameter strictly depends on the interator
 *                     implementation. It typically contains some data
 *                     used by the iterating process;
 * @param[in] new_val This parameter corresponds to the replacement for
 *                    the element, and it's provided by the user through
 *                    the diter_replace procedure.
 *
 * @see InternalIter.
 */
typedef void (*dreplace_t) (void *iterable, void *new_val);

/** Raw constructor for iterator.
 *
 * @note This function is used by the container internals. If you are
 *       just using the library, you probably will never care about this
 *       data type.
 * 
 * @param[in] nx The counterpart of the Next function of an iterator;
 * @param[in] hnx The counterpart of the Has-Next function of an
 *                iterator;
 * @param[in] rm The counterpart of the Remove function of an iterator;
 * @param[in] rep The counterpart of the Replace function of an iterator;
 * @param[in] iterable_size The size of the iterable structure, which
 *                          strictly depends on the internal
 *                          implementation of an iterator.
 *
 * @return The raw iterator, whose iterable zone must be initialized.
 *
 * @see InternalIter.
 */
diter_t *diter_new (dnext_t nx, dhasnext_t hnx, dremove_t rm,
                    dreplace_t rep, unsigned iterable_size);

/** Getter for the iterable structure
 *
 * @note This function is used by the container internals. If you are
 *       just using the library, you probably will never care about this
 *       data type.
 *
 * Given a raw iterator, this function returns the address of the memory
 * area reserved for 'iterable' structure data.
 *
 * @param[in] i The raw iterator.
 * @return The allocated area to be initialized.
 *
 * @see InternalIter.
 */
void *diter_get_iterable (diter_t *i);

/** Destructor for the raw iterator.
 *
 * Typically called after freeing the 'iterable' structure elements, if
 * any.
 *
 * @note This function is used by the container internals. If you are
 *       just using the library, you probably will never care about this
 *       data type.
 *
 * @param[in] it The iterator to be freed.
 *
 * @see InternalIter
 */
void diter_free (diter_t *it);

/*@}*/


#ifdef __cplusplus
}
#endif

#endif // __defined_dacav_dacaviter_h
