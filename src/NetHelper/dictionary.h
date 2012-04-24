/*
 *  Copyright (c) 2012 Arber Fama,
 *                     Giovanni Simoni
 *
 *  This is free software; see lgpl-2.1.txt
 */

#ifndef DICTIONARY_H
#define DICTIONARY_H

#include <netinet/in.h>
#include "config.h"
#include "sockaddr-helpers.h"
#include "client.h"

typedef struct dict * dict_t;

/* Return 1 to continue, 0 to stop scanning */
typedef int (* dict_foreach_t) (void *ctx, const sockaddr_t * addr,
                                client_t cl);

/* Destructor must support NULL as input value */
dict_t dict_new (struct tag *);

/* Returns the inner pointer to client instance */
client_t dict_search (dict_t d, const sockaddr_t *);

void dict_foreach (dict_t d, dict_foreach_t cb, void * ctx);

void dict_del (dict_t d);

#endif // DICTIONARY_H

