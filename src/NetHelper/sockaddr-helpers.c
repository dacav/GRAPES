#include "sockaddr-helpers.h"
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

static
ssize_t get_size (const sockaddr_t *s)
{
    switch (s->sa.sa_family) {
        case AF_INET:
            return sizeof(struct sockaddr_in);
        case AF_INET6:
            return sizeof(struct sockaddr_in6);
        default:
            print_err("Address analysis", NULL, EAFNOSUPPORT);
            return -1;
    }
}

void sockaddr_set_port (sockaddr_t *s, uint16_t P)
{
    switch (s->sa.sa_family) {
        case AF_INET:
            s->sin.sin_port = P;
            break;
        case AF_INET6:
            s->sin6.sin6_port = P;
            break;
        default:
            print_err("Address analysis", NULL, EAFNOSUPPORT);
            abort();
    }
}

uint16_t sockaddr_get_port (const sockaddr_t *s)
{
    uint16_t P;

    switch (s->sa.sa_family) {
        case AF_INET:
            P = s->sin.sin_port;
            break;
        case AF_INET6:
            P = s->sin6.sin6_port;
            break;
        default:
            print_err("Address analysis", NULL, EAFNOSUPPORT);
            return -1;
    }

    return htons(P);
}

size_t sockaddr_size (const sockaddr_t *s)
{
    size_t ret = get_size(s);
    if (ret == -1) {
        abort();
    }
    return ret;
}

uintptr_t sockaddr_hash (const sockaddr_t *k)
{
    uintptr_t h, g;
    size_t nbytes;
    int i;

    nbytes = sockaddr_size(k);
    h = 0;
    for (i = 0; i < nbytes; i ++) {
        h = (h << 4) + ((const uint8_t *)k)[i];
        if ((g = h & 0xf00000000) != 0) {
            h &= g >> 24;
        }
        h &= ~g;
    }
    return h;
}

int sockaddr_cmp (const sockaddr_t *sa0, const sockaddr_t *sa1)
{
    size_t size0, size1;

    size0 = sockaddr_size(sa0);
    size1 = sockaddr_size(sa1);

    return memcmp(sa0, sa1, size0 < size1 ? size0 : size1);
}

int sockaddr_equal (const sockaddr_t *sa0,
                    const sockaddr_t *sa1)
{
    return sockaddr_cmp(sa0, sa1) == 0;
}

int sockaddr_dump (void *dst, size_t dstsize, const sockaddr_t *src)
{
    size_t len;

    len = sockaddr_size(src);
    if (dstsize < len) return -1;
    memcpy(dst, (const void *)src, len);

    return len;
}

int sockaddr_undump (sockaddr_t *dst, size_t dstsize,
                     const void *src)
{
    size_t len;

    len = get_size(src);
    if (len == -1 || dstsize < len) return -1;
    memcpy((void *)dst, src, len);
    return len;
}

sockaddr_t * sockaddr_copy (sockaddr_t *dst, const sockaddr_t * src)
{
    return memcpy((void *)dst, (const void *)src, sockaddr_size(src));
}

sockaddr_t * sockaddr_dup (const sockaddr_t *src)
{
    return (sockaddr_t *) mem_dup((const void *)src, sockaddr_size(src));
}

const char * sockaddr_strrep (const sockaddr_t *s, char *buffer,
                              size_t buflen)
{
    size_t req_size;
    const void * src;

    switch (s->sa.sa_family) {
        case AF_INET:
            req_size = INET_ADDRSTRLEN;
            src = (const void *) &s->sin.sin_addr;
            break;
        case AF_INET6:
            req_size = INET6_ADDRSTRLEN;
            src = (const void *) &s->sin6.sin6_addr;
            break;
        default:
            print_err("Address to string", NULL, EAFNOSUPPORT);
            abort();
    }

    if (buflen < req_size) return NULL;
    inet_ntop(s->sa.sa_family, src, buffer, buflen);
    return buffer;
}

int sockaddr_in_init (sockaddr_t *in, const char *ipaddr,
                      uint16_t port)
{
    memset(in, 0, sizeof(sockaddr_t));

    in->sin.sin_family = AF_INET;
    in->sin.sin_port = htons(port);
    if (inet_pton(AF_INET, ipaddr, (void *)&in->sin.sin_addr) == 0) {
        print_err("Initializing sockaddr", "invalid address", 0);
        return -1;
    }

    return 0;
}

int sockaddr_send_hello (const sockaddr_t *ouraddr, int fd)
{
    return send_forced(fd, (const uint8_t *)ouraddr, sizeof(sockaddr_t));
}

int sockaddr_recv_hello (sockaddr_t *theiraddr, int fd)
{
    ssize_t len;

    if (recv_forced(fd, (uint8_t *)theiraddr, sizeof(sockaddr_t)) == -1) {
        return -1;
    }
    len = get_size(theiraddr);
    if (len == -1) {
        return -1;
    }
    return 0;
}
