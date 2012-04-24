#ifndef INBOX_H
#define INBOX_H

#include "dictionary.h"
#include "client.h"

typedef struct inbox * inbox_t;

inbox_t inbox_new ();

void inbox_del (inbox_t ib);

int inbox_empty (inbox_t ib);

int inbox_scan_dict (inbox_t ib, dict_t dict, int *user_fds,
                     struct timeval *maxwait);

client_t inbox_next (inbox_t ib);

#endif // INBOX_H

