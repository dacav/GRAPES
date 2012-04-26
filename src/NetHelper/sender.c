#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#include "sender.h"
#include "utils.h"

typedef enum {
    SND_HEADER,
    SND_MESSAGE,
    SND_IDLE,
} state_t;

struct sender {
    state_t state;

    void * buffer;
    size_t buflen;
    header_t hdr;
    size_t sent;
};

static
int can_send_more (int fd)
{
    fd_set fs;
    struct timeval zero = {
        .tv_sec = 0,
        .tv_usec = 0
    };

    FD_ZERO(&fs);
    FD_SET(fd, &fs);

    return select(fd + 1, NULL, &fs, NULL, &zero);
}

sender_t sender_new ()
{
    sender_t s;

    s = mem_new(sizeof(struct sender));

    s->buffer = NULL;
    s->buflen = 0;

    sender_reset(s);

    return s;
}

void sender_del (sender_t s)
{
    free(s->buffer);
}

sender_state_t sender_state (sender_t s)
{
    return (s->state == SND_IDLE) ? SENDER_IDLE : SENDER_BUSY;
}

void sender_reset (sender_t s)
{
    s->state = SND_IDLE;
    s->sent = 0;
}

int sender_subscribe (sender_t s, const msgbuf_t *msg)
{
    if (s->state != SND_IDLE) return -1;

    s->buffer = mem_renew(s->buffer, msg->size);
    s->buflen = msg->size;
    memcpy(s->buffer, msg->data, msg->size);
    header_set_size(&s->hdr, msg->size);
    s->sent = 0;
    s->state = SND_HEADER;

    return 0;
}

int sender_run (sender_t s, int fd)
{
    int run;
    ssize_t n;
    size_t size;
    const void * out;
    state_t next;

    run = 1;
    while (run) {
        switch (s->state) {
            case SND_HEADER:
                next = SND_MESSAGE;
                size = sizeof(header_t);
                out = (void *) &s->hdr;
                break;
            case SND_MESSAGE:
                next = SND_IDLE;
                size = s->buflen;
                out = s->buffer;
                break;
            default:
                return 1;
        }

        n = send(fd, out, size - s->sent, 0);
        if (n <= 0) return n;
        s->sent += n;
        if (s->sent == size) {
            s->sent = 0;
            s->state = next;
        }

        run = can_send_more(fd);
    }

    return 1;
}

