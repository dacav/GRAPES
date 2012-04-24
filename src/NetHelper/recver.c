#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#include "recver.h"
#include "utils.h"

typedef enum {
    RCV_HEADER,
    RCV_MESSAGE,
    RCV_COMPLETE,
} state_t;

struct recver {
    state_t state;

    struct {
        void * bytes;
        size_t len;
    } buffer;

    header_t hdr;
    size_t recvd;
};

static
int can_recv_more (int fd)
{
    fd_set fs;
    struct timeval zero = {
        .tv_sec = 0,
        .tv_usec = 0
    };

    FD_ZERO(&fs);
    FD_SET(fd, &fs);

    return select(fd + 1, &fs, NULL, NULL, &zero);
}

recver_t recver_new ()
{
    recver_t r;

    r = mem_new(sizeof(recver_t));

    r->buffer.bytes = NULL;
    r->buffer.len = 0;

    recver_reset(r);

    return r;
}

void recver_del (recver_t r)
{
    free(r->buffer.bytes);
}

recver_state_t recver_state (recver_t r)
{
    switch (r->state) {
        case RCV_HEADER:
            return (r->recvd > 0) ? RECVER_BUSY : RECVER_EMPTY;
        case RCV_MESSAGE:
            return RECVER_BUSY;
        case RCV_COMPLETE:
            return RECVER_MSG_READY;
    }
}

void recver_reset (recver_t r)
{
    r->state = RCV_HEADER;
    r->recvd = 0;
}

const msgbuf_t * recver_read (recver_t r)
{
    if (r->state != RCV_COMPLETE) return NULL;
    recver_reset(r);
    return (const msgbuf_t *) &r->buffer;
}

int recver_run (recver_t s, int fd)
{
    int run;
    ssize_t n;

    run = 1;
    while (run) {
        switch (s->state) {
            case RCV_HEADER:
                n = recv(fd, (void *)&s->hdr,
                         sizeof(header_t) - s->recvd, 0);
                if (n <= 0) return n;
                s->recvd += n;
                if (s->recvd == sizeof(header_t)) {
                    ssize_t asked = header_get_size(&s->hdr);
                    if (asked == -1) return -1;
                    s->buffer.bytes = mem_renew(s->buffer.bytes, asked);
                    s->buffer.len = asked;
                    s->recvd = 0;
                    s->state = RCV_MESSAGE;
                }
                break;
            case RCV_MESSAGE:
                n = recv(fd, s->buffer.bytes, s->buffer.len - s->recvd, 0);
                if (n <= 0) return n;
                s->recvd += n;
                if (s->recvd == s->buffer.len) {
                    s->recvd = 0;
                    s->state = RCV_COMPLETE;
                    return 1;
                }
                break;
            default:
                return 1;
        }

        run = can_recv_more(fd);
    }

    return 1;
}

