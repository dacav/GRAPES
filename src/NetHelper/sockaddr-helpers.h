#ifndef SOCKADDR_HELPERS_H
#define SOCKADDR_HELPERS_H

#include <netinet/in.h>
#include <stdint.h>

typedef union {
    struct sockaddr sa;
    struct sockaddr_storage ss;
    struct sockaddr_in sin;
    struct sockaddr_in6 sin6;
} sockaddr_t;

size_t sockaddr_size (const sockaddr_t *);

uintptr_t sockaddr_hash (const sockaddr_t *);

int sockaddr_cmp (const sockaddr_t *sa0, const sockaddr_t *sa1);

int sockaddr_equal (const sockaddr_t *sa0, const sockaddr_t *sa1);

int sockaddr_dump (void *dst, size_t dstsize, const sockaddr_t *src);

int sockaddr_undump (sockaddr_t *dst, size_t dstsize, const void *src);

sockaddr_t * sockaddr_copy (sockaddr_t *dst, const sockaddr_t * src);

sockaddr_t * sockaddr_dup (const sockaddr_t *src);

const char * sockaddr_strrep (const sockaddr_t *, char *buffer,
                              size_t buflen);

uint16_t sockaddr_get_port (const sockaddr_t *);

void sockaddr_set_port (sockaddr_t *s, uint16_t p);

int sockaddr_send_hello (const sockaddr_t *ouraddr, int fd);

int sockaddr_recv_hello (sockaddr_t *theiraddr, int fd);

/* Specific calls for sockaddr_in */

int sockaddr_in_init (sockaddr_t *in, const char *ipaddr,
                      uint16_t port);

#endif // SOCKADDR_HELPERS_H

