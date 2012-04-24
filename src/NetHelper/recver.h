#ifndef RECVER_H
#define RECVER_H

#include "nh-types.h"

typedef enum {
    RECVER_MSG_READY,
    RECVER_EMPTY,
    RECVER_BUSY
} recver_state_t;

typedef struct recver * recver_t;

recver_t recver_new ();

recver_state_t recver_state (recver_t r);

void recver_reset (recver_t r);

const msgbuf_t * recver_read (recver_t r);

int recver_run (recver_t r, int fd);

void recver_del (recver_t r);

#endif // RECVER_H

