#include "timeout.h"
#include "utils.h"

#include <sys/time.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

struct tout {
    struct timeval last_update;
    struct timeval period;
};

tout_t tout_new (const struct timeval *timeout)
{
    tout_t ret;

    if (timeout == NULL) return NULL;
    else {
        ret = mem_new(sizeof(struct tout));
        memcpy((void *)&ret->period, (const void *)timeout,
               sizeof(struct timeval));
        tout_reset(ret);

        return ret;
    }
}

tout_t tout_copy (const tout_t t)
{
    if (t == NULL) return NULL;
    else {
        tout_t ret;

        ret = mem_dup(t, sizeof(struct tout));
        tout_reset(ret);

        return ret;
    }
}

void tout_reset (tout_t t)
{
    if (t != NULL) {
        gettimeofday(&t->last_update, NULL);
    }
}

int tout_expired (tout_t t)
{
    if (t == NULL) return 0;
    else {
        struct timeval now;
        struct timeval diff;

        gettimeofday(&now, NULL);
        timersub(&now, &t->last_update, &diff);

        return timercmp(&diff, &t->period, >);
    }
}

void tout_del (tout_t t)
{
    free(t);
}

struct timeval * tout_remaining (tout_t t, struct timeval *diff)
{
    if (t == NULL) return NULL;
    else {
        struct timeval now;

        gettimeofday(&now, NULL);
        timersub(&now, &t->last_update, diff);

        return diff;
    }
}

unsigned tout_timeval_to_ms (const struct timeval *tval)
{
    return tval->tv_sec * 1000 + tval->tv_usec / 1000;
}

unsigned tout_now_ms ()
{
    struct timeval out;
    gettimeofday(&out, NULL);
    return tout_timeval_to_ms(&out);
}

