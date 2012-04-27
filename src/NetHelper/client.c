#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "client.h"
#include "utils.h"
#include "sockaddr-helpers.h"
#include "sender.h"
#include "recver.h"
#include "timeout.h"

static const unsigned CLIENT_TIMEOUT_MINUTES = 10;

struct client {
    int fd;
    sender_t send;
    recver_t recv;
    sockaddr_t remote_addr;

    tout_t tout;
    unsigned flag : 1;
};

static void set_fd (client_t cl, int fd)
{
    sender_reset(cl->send);
    recver_reset(cl->recv);
    cl->fd = fd;
}

client_t client_new ()
{
    client_t cl;
    struct timeval max_tout;

    cl = mem_new(sizeof(struct client));

    cl->fd = -1;
    cl->send = sender_new();
    cl->recv = recver_new();
    cl->flag = 0;

    max_tout.tv_sec = CLIENT_TIMEOUT_MINUTES * 60;
    max_tout.tv_usec = 0;
    cl->tout = tout_new(&max_tout);

    return cl;
}

void client_del (client_t cl)
{
    if (cl == NULL) return;

    sender_del(cl->send);
    recver_del(cl->recv);
    tout_del(cl->tout);
    if (cl->fd != -1) close(cl->fd);

    free(cl);
}

int client_connect (client_t cl, const sockaddr_t *to)
{
    int fd;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        print_err("client_connect", "socket", errno);
        return -1;
    }
    if (connect(fd, &to->sa, sockaddr_size(to)) == -1) {
        print_err("client_connect", "connect", errno);
        close(fd);
        return -1;
    }
    client_set_remote(cl, to, fd);
    return 0;
}

int client_get_fd (client_t cl)
{
    return cl->fd;
}

int client_valid (client_t cl)
{
    return recver_state(cl->recv) == RECVER_MSG_READY ||
           !invalid_fd(cl->fd);
}

int client_write (client_t cl, const msgbuf_t *msg)
{
    return sender_subscribe(cl->send, msg);
}

const msgbuf_t * client_read (client_t cl)
{
    return recver_read(cl->recv);
}

int client_has_message (client_t cl)
{
    return recver_state(cl->recv) == RECVER_MSG_READY;
}

int client_requires_sending (client_t cl)
{
    return sender_state(cl->send) == SENDER_BUSY;
}

int client_run_recv (client_t cl)
{
    int ret = recver_run(cl->recv, cl->fd);

    if (ret == 0) {
        close(cl->fd);
        cl->fd = -1;
    }
    return ret;
}

int client_run_send (client_t cl)
{
    return sender_run(cl->send, cl->fd);
}

void client_set_remote (client_t cl, const sockaddr_t *remote, int fd)
{
    sockaddr_copy(&cl->remote_addr, remote);
    set_fd(cl, fd);
}

const sockaddr_t * client_get_remote (client_t cl)
{
    return &cl->remote_addr;
}

int client_get_flag (client_t cl)
{
    return cl->flag;
}

void client_set_flag (client_t cl, int flag)
{
    cl->flag = !!flag;
}

