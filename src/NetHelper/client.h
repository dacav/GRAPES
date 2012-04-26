#ifndef CLIENT_H
#define CLIENT_H

#include "nh-types.h"
#include "sockaddr-helpers.h"

typedef struct client * client_t;

client_t client_new ();

void client_del (client_t cl);

int client_connect (client_t cl, const sockaddr_t *to);

void client_set_remote (client_t cl, const sockaddr_t *remote, int fd);

const sockaddr_t * client_get_remote (client_t cl);

const msgbuf_t * client_read (client_t cl);

int client_run_recv (client_t cl);

int client_run_send (client_t cl);

int client_write (client_t cl, const msgbuf_t *msg);

int client_get_fd (client_t cl);

int client_valid (client_t cl);

int client_has_message (client_t cl);

int client_get_flag (client_t cl);

void client_set_flag (client_t cl, int flag);

#endif // CLIENT_H

