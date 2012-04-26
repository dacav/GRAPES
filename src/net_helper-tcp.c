/*
 *  Copyright (c) 2012 Arber Fama,
 *                     Giovanni Simoni
 *
 *  This is free software; see lgpl-2.1.txt
 */

#include <stdio.h>

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/select.h>
#include <sys/time.h>

#include "net_helper.h"
#include "config.h"

#include "NetHelper/sender.h"
#include "NetHelper/recver.h"
#include "NetHelper/dictionary.h"
#include "NetHelper/timeout.h"
#include "NetHelper/sockaddr-helpers.h"
#include "NetHelper/async-operations.h"

typedef struct {
    dict_t neighbors;
    int srvfd;
    aqueue_t aqueue;
} local_t;

typedef struct nodeID {
    unsigned refcount;
    sockaddr_t addr;
    local_t *loc;
} nodeid_t;

static const char *   CONF_KEY_BACKLOG = "tcp_backlog";
static const unsigned DEFAULT_BACKLOG = 50;
static const size_t   ERR_BUFLEN = 64;

static
int tcp_serve (int backlog, sockaddr_t *addr)
{
    int fd;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        print_err("tcp_serve", "socket", errno);
        return -1;
    }

    if (bind(fd, &addr->sa, sizeof(struct sockaddr_in))) {
        print_err("tcp_serve", "bind", errno);
        close(fd);
        return -1;
    }

    if (listen(fd, backlog) == -1) {
        print_err("tcp_serve", "listen", errno);
        close(fd);
        return -1;
    }

    return fd;
}

struct nodeID *nodeid_dup (struct nodeID *s)
{
    s->refcount ++;
    return s;
}

/*
* @return -1, 0 or 1, depending on the relation between  s1 and s2.
*/
int nodeid_cmp (const nodeid_t *s1, const nodeid_t *s2)
{
    if (s1 == s2) return 0;
    return sockaddr_cmp(&s1->addr, &s2->addr);
}

/* @return 1 if the two nodeID are identical or 0 if they are not. */
int nodeid_equal (const nodeid_t *s1, const nodeid_t *s2)
{
    if (s1 == s2) return 1;
    return sockaddr_equal(&s1->addr, &s2->addr);
}

struct nodeID *create_node (const char *ipaddr, int port)
{
    nodeid_t *ret = mem_new(sizeof(struct nodeID));

    /* Initialization of address */
    memset(&ret->addr, 0, sizeof(sockaddr_t));
    ret->addr.sa.sa_family = AF_INET;
    sockaddr_set_port(&ret->addr, port);

    if (ipaddr == NULL) {
        /* In case of server, specifying NULL will allow anyone to
         * connect. */
        ret->addr.sin.sin_addr.s_addr = INADDR_ANY;
    } else if (inet_pton(AF_INET, ipaddr,
                        (void *)&ret->addr.sin.sin_addr) == 0) {
        fprintf(stderr, "Invalid ip address %s\n", ipaddr);
        free(ret);
        return NULL;
    }

    ret->loc = NULL;
    ret->refcount = 1;

    return ret;
}

void nodeid_free (struct nodeID *s)
{
    if (s == NULL) return;

    s->refcount --;
    if (s->refcount == 0) {
        local_t *local = s->loc;

        if (local != NULL) {
            dict_del(local->neighbors);
            if (local->srvfd != -1) close(local->srvfd);
            aqueue_del(local->aqueue);
            free(local);
        }
        free(s);
    }
}

struct nodeID * net_helper_init (const char *ipaddr, int port,
                                 const char *config)
{
    nodeid_t *self;
    struct tag *cfg_tags;
    int backlog;
    local_t *local;

    self = create_node(ipaddr, port);
    if (self == NULL) {
        return NULL;
    }

    self->loc = local = mem_new(sizeof(local_t));

    /* Default settings */
    backlog = DEFAULT_BACKLOG;

    /* Reading settings */
    cfg_tags = NULL;
    if (config) {
        cfg_tags = config_parse(config);
    }
    if (cfg_tags) {
        /* FIXME: this seems not to work! Testing needed */
        config_value_int_default(cfg_tags, CONF_KEY_BACKLOG, &backlog,
                                 DEFAULT_BACKLOG);
    }
    local->neighbors = dict_new(cfg_tags);
    free(cfg_tags);

    local->aqueue = aqueue_new();
    local->srvfd = tcp_serve(backlog, &self->addr);
    if (local->srvfd == -1) {
        nodeid_free(self);
        return NULL;
    }

    return self;
}

void bind_msg_type(uint8_t msgtype) {}

int send_to_peer(const struct nodeID *self, struct nodeID *to,
                 const uint8_t *buffer_ptr, int buffer_size)
{
    client_t cl;
    local_t *loc;
    struct timeval zero = {
        .tv_sec = 0,
        .tv_usec = 0
    };
    const msgbuf_t msg = {
        .data = buffer_ptr,
        .size = buffer_size
    };

    assert(self->loc != NULL);

    loc = self->loc;

    cl = dict_search(loc->neighbors, &to->addr);
    if (!client_valid(cl)) {
        int clfd;

        if (client_connect(cl, &to->addr) == -1) {
            return -1;
        }
        clfd = client_get_fd(cl);
        if (sockaddr_send_hello(&self->addr, clfd) == -1) {
            close(clfd);
            return -1;
        }
    }

    if (client_write(cl, &msg) == -1) return -1;
    if (aqueue_scan_dict(loc->aqueue, loc->neighbors,
                         loc->srvfd, NULL, &zero) == -1) {
        return -1;
    }
    return buffer_size;
}

int recv_from_peer(const struct nodeID *self, struct nodeID **remote,
                   uint8_t *buffer_ptr, int buffer_size)
{
    local_t *loc;
    client_t sender;
    nodeid_t *sender_node;
    const msgbuf_t *msg;

    assert(self->loc != NULL);
    loc = self->loc;

    while (aqueue_empty(loc->aqueue)) {
        if (aqueue_scan_dict(loc->aqueue, loc->neighbors, loc->srvfd,
                             NULL, NULL) == -1) {
            return -1;
        }
    }

    sender = aqueue_next(loc->aqueue);
    msg = client_read(sender);
    assert(msg != NULL);

    if (msg->size > buffer_size) {
        print_err("recv_from_peer", NULL, ENOBUFS);
    }
    memcpy((void *)buffer_ptr, msg->data, msg->size);

    sender_node = create_node(NULL, 0);
    sockaddr_copy(&sender_node->addr, client_get_remote(sender));
    *remote = sender_node;

    return (int) msg->size;
}

int wait4data(const struct nodeID *self, struct timeval *tout,
              int *user_fds)
{
    local_t *loc;
    struct timeval tmp;
    int res, ret;
    tout_t T;

    assert(self->loc != NULL);
    loc = self->loc;

    T = tout_new(tout);
    ret = 0;
    while (!tout_expired(T) && aqueue_empty(loc->aqueue)) {
        res = aqueue_scan_dict(loc->aqueue, loc->neighbors, loc->srvfd,
                               NULL, tout_remaining(T, &tmp));
        if (res == -1) {
            tout_del(T);
            return -1;
        }
        ret |= res;
    }
    tout_del(T);

    return ret || !aqueue_empty(loc->aqueue);
}

struct nodeID *nodeid_undump (const uint8_t *b, int *len)
{
    nodeid_t *node;

    node = create_node(NULL, 0);
    sockaddr_undump(&node->addr, sizeof(sockaddr_t), (const void *)b);

    return node;
}

int nodeid_dump (uint8_t *b, const struct nodeID *s,
                 size_t max_write_size)
{
    return sockaddr_dump((void *)b, max_write_size, &s->addr);
}

int node_ip(const struct nodeID *s, char *ip, int len)
{
    return sockaddr_strrep(&s->addr, ip, len) == NULL ? 0 : -1;
}

int node_addr(const struct nodeID *s, char *addr, int len)
{
    if (len < INET_ADDRSTRLEN + 6) {  // 2^16 + ':' = 5 cyphers + 1 char
        return -1;
    }
    node_ip(s, addr, len);
    sprintf(addr + strlen(addr), ":%hu", sockaddr_get_port(&s->addr));
    return 0;
}

