#include "async-operations.h"

#include <dacav/dacav.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <sys/select.h>

#include "utils.h"
#include "sockaddr-helpers.h"
#include "dictionary.h"

struct aqueue {
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
    aqueue_t q = (aqueue_t) ctx;

    fd = client_get_fd(cl);
    FD_SET(fd, &q->read);
    FD_SET(fd, &q->write);
    if (fd > q->maxfd) {
        q->maxfd = fd;
    }

    return 1;
}

static
int collect_callback (void *ctx, const sockaddr_t *addr, client_t cl)
{
    int fd;
    aqueue_t q = (aqueue_t) ctx;

    fd = client_get_fd(cl);
    if (FD_ISSET(fd, &q->read)) {
        q->n_active --;
        client_run_recv(cl);
    }

    if (FD_ISSET(fd, &q->write)) {
        q->n_active --;
        client_run_send(cl);
    }
    assert(q->n_active >= 0);

    if (client_has_message(cl)) {
        q->queue = dlist_append(q->queue, (void *)cl);
    }
    return q->n_active > 0;
}

static
int accept_connections (dict_t d, int srvfd)
{
    fd_set s;
    int newfd;
    socklen_t len;
    sockaddr_t remote;
    client_t cl;

    FD_ZERO(&s);

    for(;;) {
        struct timeval zero = {
            .tv_sec = 0,
            .tv_usec = 0
        };

        FD_SET(srvfd, &s);

        switch (select(srvfd + 1, &s, NULL, NULL, &zero)) {
            case -1:
                return -1;
            case 0:
                return 0;
            default:
                len = sizeof(struct sockaddr_in);
                newfd = accept(srvfd, &remote.sa, &len);
                if (newfd == -1) {
                    print_err("client_accept", "accept", errno);
                    return -1;
                }
                if (sockaddr_recv_hello(&remote, newfd) == -1) {
                    return -1;
                }
                client_set_remote(dict_search(d, &remote),
                                  &remote, newfd);
        }
    }
}

aqueue_t aqueue_new ()
{
    aqueue_t q;

    q = mem_new(sizeof(struct aqueue));
    q->queue = dlist_new();

    return q;
}

int aqueue_empty (aqueue_t q)
{
    return dlist_empty(q->queue);
}

void aqueue_del (aqueue_t q)
{
    if (q == NULL) return;
    /* Note: client_t items are already stored in the dictionary. They are
     *       just kept in a queue here. And yes, this is safe */
    dlist_free(q->queue, NULL);
    free(q);
}

int aqueue_scan_dict (aqueue_t q, dict_t dict, int srv_fd, int *user_fds,
                      struct timeval *maxwait)
{
    int i, retval, sr;

    FD_ZERO(&q->read);
    FD_ZERO(&q->write);

    FD_SET(srv_fd, &q->read);
    q->maxfd = srv_fd;

    if (user_fds != NULL) {
        for (i = 0; user_fds[i] != -1; i ++) {
            int fd = user_fds[i];

            FD_SET(fd, &q->read);
            if (fd > q->maxfd) {
                q->maxfd = fd;
            }
        }
    }

    /* Register all neighbors in the lists */
    dict_foreach(dict, build_sets_callback, (void *)q);

    sr = select(q->maxfd + 1, &q->read, &q->write, NULL, maxwait);
    if (sr <= 0) {
        if (sr == -1) print_err("aqueue_scan_dict", "select", errno);
        return sr;     /* Maybe an error, or just timeout */
    }

    if (FD_ISSET(srv_fd, &q->read)) {
        sr --;
        accept_connections(dict, srv_fd);
    }

    retval = 0;
    if (user_fds != NULL) {
        for (i = 0; sr > 0 && user_fds[i] != -1; i ++) {
            int fd = user_fds[i];

            if (FD_ISSET(fd, &q->read)) {
                user_fds[i] = -2;
                retval = 1;
                sr --;
            }
        }
        assert(sr >= 0);
    }

    q->n_active = sr;
    dict_foreach(dict, collect_callback, (void *)q);

    return retval;
}

client_t aqueue_next (aqueue_t q)
{
    client_t ret;

    if (dlist_empty(q->queue)) return NULL;
    q->queue = dlist_pop(q->queue, (void **) &ret);
    return ret;
}

