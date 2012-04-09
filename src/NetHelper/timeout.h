#ifndef TIMEOUT_H
#define TIMEOUT_H

#include <sys/time.h>

typedef struct tout * tout_t;

tout_t tout_new (const struct timeval *timeout); 

tout_t tout_copy (const tout_t t); 

void tout_reset (tout_t t);

int tout_expired (tout_t t);

void tout_del (tout_t t);

#endif // TIMEOUT_H

