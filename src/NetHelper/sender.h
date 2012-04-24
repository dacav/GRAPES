#ifndef SENDER_H
#define SENDER_H

#include "utils.h"

typedef enum {
    SENDER_IDLE,
    SENDER_BUSY
} sender_state_t;

typedef struct sender * sender_t;

sender_t sender_new ();

sender_state_t sender_state (sender_t s);

void sender_reset (sender_t s);

int sender_subscribe (sender_t s, const msgbuf_t *msg);

int sender_run (sender_t s, int fd);

void sender_del (sender_t s);

#endif // SENDER_H

