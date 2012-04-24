#include "inbox.h"

#include <dacav/dacav.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/select.h>

#include "utils.h"
#include "sockaddr-helpers.h"
#include "dictionary.h"

struct inbox {
    dlist_t * queue;
    fd_set read;
    fd_set write;
    int maxfd;
    int n_active;
};

static
int build_sets_callback (void *ctx, const sockaddr_t *addr, client_t cl)
{
    int fd;
    inbox_t ib = (inbox_t) ctx;

    fd = client_get_fd(cl);

    FD_SET(fd, &ib->read);
    FD_SET(fd, &ib->write);
    if (fd > ib->maxfd) {
        ib->maxfd = fd;
    }

    return 1;
}

static
int collect_callback (void *ctx, const sockaddr_t *addr, client_t cl)
{
    int fd;
    inbox_t ib = (inbox_t) ctx;

    fd = client_get_fd(cl);
    if (FD_ISSET(fd, &ib->read)) {
        ib->n_active --;
        client_run_recv(cl);
    }

    if (FD_ISSET(fd, &ib->write)) {
        ib->n_active --;
        client_run_send(cl);
    }
    assert(ib->n_active >= 0);

    if (client_has_message(cl)) {
        ib->queue = dlist_append(ib->queue, (void *)cl);
    }
    return ib->n_active > 0;
}

inbox_t inbox_new ()
{
    inbox_t ib;

    ib = mem_new(sizeof(struct inbox));
    ib->queue = dlist_new();

    return ib;
}

int inbox_empty (inbox_t ib)
{
    return dlist_empty(ib->queue);
}

void inbox_del (inbox_t ib)
{
    if (ib == NULL) return;
    /* Note: client_t items are already stored in the dictionary. They are
     *       just kept in a queue here. And yes, this is safe */
    dlist_free(ib->queue, NULL);
    free(ib);
}

int inbox_scan_dict (inbox_t ib, dict_t dict, int *user_fds,
                     struct timeval *maxwait)
{
    int i, retval, sr;

    FD_ZERO(&ib->read);
    FD_ZERO(&ib->write);
    ib->maxfd = -1;

    if (user_fds != NULL) {
        for (i = 0; user_fds[i] != -1; i ++) {
            int fd = user_fds[i];

            FD_SET(fd, &ib->read);
            if (fd > ib->maxfd) {
                ib->maxfd = fd;
            }
        }
    }

    dict_foreach(dict, build_sets_callback, (void *)ib);
    if (ib->maxfd == -1) {
        return 0;       /* Nothing to work on */
    }
    sr = select(ib->maxfd + 1, &ib->read, &ib->write, NULL, maxwait);
    if (sr <= 0) {
        if (sr == -1) print_err("inbox_scan_dict", "select", errno);
        return sr;     /* Maybe an error, or just timeout */
    }

    retval = 0;
    if (user_fds != NULL) {
        for (i = 0; user_fds[i] != -1; i ++) {
            int fd = user_fds[i];

            if (FD_ISSET(fd, &ib->read)) {
                user_fds[i] = -2;
                retval = 1;
                sr --;
            }
        }
        assert(sr >= 0);
        if (sr == 0) return retval;
    }

    ib->n_active = sr;
    dict_foreach(dict, collect_callback, (void *)ib);

    return retval;
}

client_t inbox_next (inbox_t ib)
{
    client_t ret;

    if (dlist_empty(ib->queue)) return NULL;
    ib->queue = dlist_pop(ib->queue, (void **) &ret);
    return ret;
}

