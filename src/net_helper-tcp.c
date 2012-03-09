/*
 *  Copyright (c) 2012 Arber Fama,
 *                     Giovanni Simoni
 *
 *  This is free software; see lgpl-2.1.txt
 */


#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "net_helper.h"
#include "config.h"
#include "NetHelper/fair.h"
#include "NetHelper/dictionary.h"

/* -- Internal data types -------------------------------------------- */

typedef struct {
    int fd;
    int sendretry;              // Number of retry in sending data
    dict_t neighbours;
    connection_t cached_peer;   // Cached during wait4data, used in
                                // recv_from_peer
} local_info_t;

typedef struct nodeID {
    struct sockaddr_in addr;
    struct {
        char ip[INET_ADDRSTRLEN];           // ip address
        char ip_port[INET_ADDRSTRLEN + 6];  // ip:port 2^16 -> 5 cyphers.
    } repr;                         // reentrant string representations
    local_info_t *local;            // non-NULL only for local node
} nodeid_t;

/* -- Internal functions --------------------------------------------- */

static int tcp_connect (struct sockaddr_in *to, int *out_fd, int *e);
static int tcp_serve (nodeid_t *sd, int backlog, int *e);
static void print_err (int e, const char *msg);
static int get_peer (dict_t neighbours, struct sockaddr_in *addr);
static int would_block (int e);
static int dead_filedescriptor (int e);
static nodeid_t * addr_to_nodeid (const struct sockaddr_in *addr);

/* -- Constants ------------------------------------------------------ */

static const char *   CONF_KEY_BACKLOG = "tcp_backlog";
static const unsigned DEFAULT_BACKLOG = 50;
static const char *   CONF_KEY_SENDRETRY = "tcp_send_retry";
static const int      DEFAULT_SENDRETRY = 3;

static const size_t   ERR_BUFLEN = 64;

/* -- Interface exported symbols ------------------------------------- */

struct nodeID *nodeid_dup (struct nodeID *s)
{
    nodeid_t *ret;

    /* Local nodeID cannot be duplicated! */
    assert(s->local == NULL);

    ret = malloc(sizeof(nodeid_t));
    if (ret == NULL) return NULL;
    memcpy(ret, s, sizeof(nodeid_t));
    ret->local = NULL;

    return ret;
}

/*
* @return -1, 0 or 1, depending on the relation between  s1 and s2.
*/
int nodeid_cmp (const nodeid_t *s1, const nodeid_t *s2)
{
    return memcmp(&s1->addr, &s2->addr, sizeof(struct sockaddr_in));
}

/* @return 1 if the two nodeID are identical or 0 if they are not. */
int nodeid_equal (const nodeid_t *s1, const nodeid_t *s2)
{
    return nodeid_cmp(s1, s2) == 0;
}

struct nodeID *create_node (const char *IPaddr, int port)
{
    nodeid_t *ret = malloc(sizeof(nodeid_t));
    if (ret == NULL) {
        return NULL;
    }

    ret->addr.sin_family = AF_INET;
    ret->addr.sin_port = htons(port);

    if (IPaddr == NULL) {
        /* In case of server, specifying NULL will allow anyone to
         * connect. */
        ret->addr.sin_addr.s_addr = INADDR_ANY;
        inet_ntop(AF_INET, (const void *)&ret->addr.sin_addr,
                  ret->repr.ip, INET_ADDRSTRLEN);
    } else {
        if (inet_pton(AF_INET, IPaddr,
                         (void *)&ret->addr.sin_addr) == 0) {
            fprintf(stderr, "Invalid ip address %s\n", IPaddr);
            free(ret);
            return NULL;
        }
        strcpy(ret->repr.ip, IPaddr);
    }
    sprintf(ret->repr.ip_port, "%s:%hu", ret->repr.ip, (uint16_t)port);

    return ret;
}

void nodeid_free (struct nodeID *s)
{
    local_info_t *local = s->local = s->local;
    if (local != NULL) {
        dict_delete(local->neighbours);
        close(local->fd);
        free(local);
    }
    free(s);
}

struct nodeID * net_helper_init (const char *IPaddr, int port,
                                 const char *config)
{
    nodeid_t *self;
    struct tag *cfg_tags;
    int backlog;
    int e;
    local_info_t *local;

    self = create_node(IPaddr, port);
    if (self == NULL) {
        return NULL;
    }

    cfg_tags = NULL;
    if (config) {
        cfg_tags = config_parse(config);
    }

    self->local = local = malloc(sizeof(local_info_t));
    if (local == NULL) {
        nodeid_free(self);
        free(cfg_tags);
        return NULL;
    }

    local->neighbours = dict_new(AF_INET, 1, cfg_tags);

    local->sendretry = DEFAULT_SENDRETRY;
    backlog = DEFAULT_BACKLOG;
    if (cfg_tags) {
        config_value_int_default(cfg_tags, CONF_KEY_BACKLOG, &backlog,
                                 DEFAULT_BACKLOG);
        config_value_int_default(cfg_tags, CONF_KEY_SENDRETRY,
                                 &local->sendretry,
                                 DEFAULT_SENDRETRY);
    }

    if (tcp_serve(self, backlog, &e) < 0) {
        print_err(e, "creating server");
        nodeid_free(self);
        return NULL;
    }

    local->cached_peer.fd = -1;

    free(cfg_tags);
    return self;
}

void bind_msg_type(uint8_t msgtype) {}

int send_to_peer(const struct nodeID *from, struct nodeID *to,
                 const uint8_t *buffer_ptr, int buffer_size)
{
    int retry;
    ssize_t sent;
    int peer_fd;
    local_info_t *local;

    if (buffer_size <= 0) {
        return 0;
    }

    local = from->local;
    assert(local != NULL);          // TODO: remove after testing
    assert(to->local == NULL);      // TODO: ditto

    peer_fd = get_peer(local->neighbours, &to->addr);
    if (peer_fd == -1) {
        return -1;
    }

    retry = local->sendretry;
    sent = 0;
    while (retry && buffer_size > 0) {
        ssize_t n = send(peer_fd, buffer_ptr, buffer_size, MSG_DONTWAIT);
        if (n <= 0) {
            if (would_block(errno)) {
                retry --;
            } else if (dead_filedescriptor(errno)) {
                dict_remove(local->neighbours, (struct sockaddr *)&to->addr);
                retry = 0;
                if (sent == 0) sent = -1;
            }
        } else {
            buffer_ptr += n;
            sent += n;
            buffer_size -= n;
        }
    }

    return sent;
}

int recv_from_peer(const struct nodeID *self, struct nodeID **remote,
                   uint8_t *buffer_ptr, int buffer_size)
{
    local_info_t *local;
    connection_t *peer;
    int retval;

    assert(self->local != NULL);        // TODO: remove after testing
    local = self->local;
    peer = &local->cached_peer;
    if (peer->fd == -1) {
        int err;
        fd_set fds;

        /* No cache from wait4data */
        FD_ZERO(&fds);
        if (fair_select(local->neighbours, NULL, &fds, 0,
                        peer, &err) == -1) {
            print_err(err, "recv select");
        }
    }

    if ((*remote = addr_to_nodeid((const struct sockaddr_in *)peer->addr))
            == NULL) {
        return -1;
    }

    retval = buffer_size;
    while (retval > 0 && buffer_size > 0) {
        ssize_t n;

        switch (n = recv(peer->fd, buffer_ptr, buffer_size, 0)) {
            case -1:
                print_err(errno, "receiving");
            case 0:
                dict_remove(local->neighbours, peer->addr);
                retval = -1;
                break;
            default:
                buffer_size -= n;
                buffer_ptr += n;
        }
    }

    peer->fd = -1;
    return retval;
}

int wait4data(const struct nodeID *self, struct timeval *tout,
              int *user_fds)
{
    fd_set fdset;
    int maxfd;
    int err;
    int i;
    local_info_t *local = self->local;

    assert(local != NULL);        // TODO: remove after testing
    FD_ZERO(&fdset);
    maxfd = -1;

    if (user_fds != NULL) {
        for (i = 0; user_fds[i] != -1; i++) {
            FD_SET(user_fds[i], &fdset);
            if (user_fds[i] > maxfd) {
                maxfd = user_fds[i];
            }
        }
    }

    switch (fair_select(local->neighbours, tout, &fdset, maxfd + 1,
                        &local->cached_peer, &err)) {
        case 0:
            return 0;
        case -1:
            print_err(err, "wait4data select");
            return -1;
        default:
            if (local->cached_peer.fd == -1) {
                /* Externally provided file descriptor unlocked select */
                for (i = 0; user_fds[i] != -1; i++) {
                    if (!FD_ISSET(user_fds[i], &fdset)) {
                        user_fds[i] = -2;
                    }
                }
                return 2;
            }
            return 1;
    }
}

struct nodeID *nodeid_undump (const uint8_t *b, int *len)
{
    nodeid_t *ret = malloc(sizeof(nodeid_t));
    if (ret == NULL) {
        return NULL;
    }

    memcpy((void *)&ret->addr, (const void *)b,
            sizeof(struct sockaddr_in));
    inet_ntop(AF_INET, (const void *)b, ret->repr.ip,
              sizeof(struct sockaddr_in));
    sprintf(ret->repr.ip_port, "%s:%hu", ret->repr.ip,
            ret->addr.sin_port);
    ret->local = NULL;

    return ret;
}

int nodeid_dump (uint8_t *b, const struct nodeID *s,
                 size_t max_write_size)
{
    assert(s->local == NULL);   // TODO: remove after testing

    if (max_write_size < sizeof(struct sockaddr_in)) {
        return -1;
    }
    memcpy((void *)b, (const void *)&s->addr, sizeof(struct sockaddr_in));
    return sizeof(struct sockaddr_in);
}

const char *node_ip(const struct nodeID *s)
{
    return s->repr.ip;
}

const char *node_addr (const struct nodeID *s)
{
    return s->repr.ip_port;
}

/* -- Internal functions --------------------------------------------- */

static
int tcp_connect (struct sockaddr_in *to, int *out_fd, int *e)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd == -1) {
        if (e) *e = errno;
        return -1;
    }

    if (connect(fd, (struct sockaddr *) &to,
                sizeof(struct sockaddr_in)) == -1) {
        if (e) *e = errno;
        close(fd);
        return -2;
    }
    *out_fd = fd;

    return 0;
}

static
int tcp_serve (nodeid_t *sd, int backlog, int *e)
{
    int fd;
    assert(sd->local != NULL);  // TODO: remove when it works.

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        if (e) *e = errno;
        return -1;
    }

    if (bind(fd, (struct sockaddr *) &sd->addr,
             sizeof(struct sockaddr_in))) {
        if (e) *e = errno;
        close(fd);
        return -2;
    }

    if (listen(fd, backlog) == -1) {
        if (e) *e = errno;
        close(fd);
        return -3;
    }
    sd->local->fd = fd;

    return 0;
}

/* perror-like with parametrized error */
static
void print_err (int e, const char *msg)
{
    char buf[ERR_BUFLEN];
    strerror_r(e, buf, ERR_BUFLEN);
    if (msg) {
        fprintf(stderr, "net-helper-tcp: %s: %s\n", msg, buf);
    } else {
        fprintf(stderr, "net_helper-tcp: %s\n", buf);
    }
}

/* TODO: this can be optimized using new features for dictionaries */
static
int get_peer (dict_t neighbours, struct sockaddr_in *addr)
{
    peer_info_t peer;

    if (dict_lookup(neighbours, (struct sockaddr *) addr, &peer) == -1) {
        /* We don't have the address stored, thus we need to connect */

        int err;
        if (tcp_connect((struct sockaddr_in *)&addr, &peer.fd,
                        &err) == -1) {
            print_err(err, "connecting to peer");
            return -1;
        }

        dict_insert(neighbours, (struct sockaddr *) &addr, peer.fd);
    }

    return peer.fd;
}

static inline
int would_block (int e)
{
    return e == EAGAIN||
           e == EWOULDBLOCK;
}

static inline
int dead_filedescriptor (int e)
{
    return e == ECONNRESET ||
           e == EBADF ||
           e == ENOTCONN;
}

static
nodeid_t * addr_to_nodeid (const struct sockaddr_in *addr)
{
    nodeid_t *ret;

    ret = malloc(sizeof(nodeid_t));
    if (ret == NULL) return NULL;

    ret->local = NULL;
    memcpy((void *)&ret->addr, (const void *)addr,
           sizeof(struct sockaddr_in));
    inet_ntop(AF_INET, (const void *)&ret->addr.sin_addr,
              ret->repr.ip, INET_ADDRSTRLEN);
    sprintf(ret->repr.ip_port, "%s:%hu", ret->repr.ip,
            addr->sin_port);

    return ret;
}

