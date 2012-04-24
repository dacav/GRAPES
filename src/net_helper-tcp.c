/*
 *  Copyright (c) 2012 Arber Fama,
 *                     Giovanni Simoni
 *
 *  This is free software; see lgpl-2.1.txt
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/select.h>

#include "net_helper.h"
#include "config.h"

#include "NetHelper/sender.h"
#include "NetHelper/recver.h"
#include "NetHelper/dictionary.h"
#include "NetHelper/sockaddr-helpers.h"
#include "NetHelper/inbox.h"

typedef struct {
    dict_t neighbors;
    int servfd;
    inbox_t inbox;

    unsigned refcount;
} local_t;

typedef struct nodeID {
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

static
int accept_connections (dict_t d, int srvfd)
{
    fd_set s;
    int newfd;
    struct timeval zero = {0,0};
    socklen_t len;
    sockaddr_t remote;
    client_t cl;

    FD_ZERO(&s);
    FD_SET(srvfd, &s);

    for(;;) {
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

struct nodeID *nodeid_dup (struct nodeID *s)
{
    nodeid_t *ret;

    if (s->loc == NULL) {
        return mem_dup(s, sizeof(struct nodeID));
    } else {
        /* This reference counter trick will avoid copying around of the
         * nodeid for the local host */
        s->loc->refcount ++;
        return s;
    }
}

/*
* @return -1, 0 or 1, depending on the relation between  s1 and s2.
*/
int nodeid_cmp (const nodeid_t *s1, const nodeid_t *s2)
{
    return sockaddr_cmp(&s1->addr, &s2->addr);
}

/* @return 1 if the two nodeID are identical or 0 if they are not. */
int nodeid_equal (const nodeid_t *s1, const nodeid_t *s2)
{
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

    return ret;
}

void nodeid_free (struct nodeID *s)
{
    local_t *local = s->loc;

    if (local != NULL) {
        local->refcount --;
        if (local->refcount == 0) {
            dict_del(local->neighbors);
            if (local->servfd != -1) close(local->servfd);
            inbox_del(local->inbox);
        }
    }
    free(s);
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

    local->inbox = inbox_new();
    local->refcount = 1;
    local->servfd = tcp_serve(backlog, &self->addr);
    if (local->servfd == -1) {
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
    if (accept_connections(loc->neighbors, loc->servfd) == -1) {
        return -1;
    }
    cl = dict_search(loc->neighbors, &to->addr);

    if (!client_valid(cl)) {
        int clfd;

        client_connect(cl, &to->addr);
        clfd = client_get_fd(cl);
        if (sockaddr_send_hello(&self->addr, clfd) == -1) {
            close(clfd);
            return -1;
        }
    }

    inbox_scan_dict(loc->inbox, loc->neighbors, NULL, &zero);
    if (client_write(cl, &msg) == -1) {
        return -1;          /* It might be valid, but busy */
    }
    inbox_scan_dict(loc->inbox, loc->neighbors, NULL, &zero);
    return 0;
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

    if (accept_connections(loc->neighbors, loc->servfd) == -1) {
        return -1;
    }
 
    while (inbox_empty(loc->inbox)) {
        inbox_scan_dict(loc->inbox, loc->neighbors, NULL, NULL);
    }

    sender = inbox_next(loc->inbox);
    msg = client_read(sender);

    assert(msg != NULL);
    memcpy((void *)buffer_ptr, msg->data, msg->size);

    sender_node = create_node(NULL, 0);
    sockaddr_copy(&sender_node->addr, client_get_remote(sender));

    *remote = sender_node;
    return (int) msg->size;
}

int wait4data(const struct nodeID *self, struct timeval *tout,
              int *user_fds)
{
    unsigned now, time_limit;
    local_t *loc;

    assert(self->loc != NULL);
    loc = self->loc;

    if (accept_connections(loc->neighbors, loc->servfd) == -1) {
        return -1;
    }
 
    if (!inbox_empty(loc->inbox)) return 1;
    switch (inbox_scan_dict(loc->inbox, loc->neighbors, user_fds, tout)) {
        case -1:
            return -1;
        case 0:
            return inbox_empty(loc->inbox);
        case 1:
            return 1;
    }
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

