/*
 *  Copyright (c) 2012 Arber Fama,
 *                     Giovanni Simoni
 *
 *  This is free software; see lgpl-2.1.txt
 */

#ifndef NH_TYPES_H
#define NH_TYPES_H

#include <arpa/inet.h>
#include <stdint.h>

typedef struct {
    uint32_t size;
} header_t;

typedef struct {
    const void * data;
    size_t size;
} msgbuf_t;

#endif // NH_TYPES_H

