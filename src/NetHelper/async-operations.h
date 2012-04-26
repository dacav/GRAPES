#ifndef ASYNC_OPERATIONS_H
#define ASYNC_OPERATIONS_H

#include "dictionary.h"

typedef struct aqueue * aqueue_t;

aqueue_t aqueue_new ();


/**
 * @retval 0 on normal execution;
 * @retval 1 if user_fds are involved in selects;
 * @retval -1 on error.
 */
int aqueue_scan_dict (aqueue_t q, dict_t dict, int srv_fd, int *user_fds,
                      struct timeval *maxwait);

int aqueue_empty (aqueue_t q);

void aqueue_del (aqueue_t q);

client_t aqueue_next (aqueue_t q);

#endif // ASYNC_OPERATIONS_H
