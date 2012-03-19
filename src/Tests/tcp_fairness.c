#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>

#include "net_helper.h"

static const uint16_t START_PORT = 9400;
static const char TEST_STRING[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec a diam"
    "lectus. Sed sit amet ipsum mauris. Maecenas congue ligula ac quam"
    "viverra nec consectetur ante hendrerit. Donec et mollis dolor."
    "Praesent et diam eget libero egestas mattis sit amet vitae augue. Nam"
    "tincidunt congue enim, ut porta lorem lacinia consectetur. Donec ut"
    "libero sed arcu vehicula ultricies a non tortor. Lorem ipsum dolor sit"
    "amet, consectetur adipiscing elit. Aenean ut gravida lorem. Ut turpis"
    "felis, pulvinar a semper sed, adipiscing id dolor. Pellentesque auctor"
    "nisi id magna consequat sagittis. Curabitur dapibus enim sit amet elit"
    "pharetra tincidunt feugiat nisl imperdiet. Ut convallis libero in urna"
    "ultrices accumsan. Donec sed odio eros. Donec viverra mi quis quam"
    "pulvinar at malesuada arcu rhoncus. Cum sociis natoque penatibus et"
    "magnis dis parturient montes, nascetur ridiculus mus. In rutrum"
    "accumsan ultricies.  Mauris vitae nisi at sem facilisis semper ac in"
    "est.";

static const size_t READ_BUFSIZE = 10;
static const int MAX_RETRY = 5;
static const size_t N_SENDERS = 10;

void run_receiver ()
{
    struct nodeID *self;
    int retry;
    size_t cumulate;

    self = net_helper_init("127.0.0.1", START_PORT, "tcp_backlog=10");
    fprintf(stderr, "Receiver ready\n");

    retry = MAX_RETRY;
    cumulate = 0;
    while (retry) {
        struct timeval tout = {0, 500000};

        if (wait4data(self, &tout, NULL) == 1) {
            struct nodeID *remote;
            uint8_t buffer[READ_BUFSIZE];
            ssize_t N;

            N = recv_from_peer(self, &remote, buffer, READ_BUFSIZE);
            if (N < 0) {
                retry --;
                fprintf(stderr, "Recv failed, retry=%i\n", retry);
            } else {
                fprintf(stderr, "Received %i bytes\n", (int)N);
                cumulate += N;
                retry = MAX_RETRY;
            }
            nodeid_free(remote);
        } else {
            retry --;
            fprintf(stderr, "Timeout, retry=%i\n", retry);
        }
    }

    printf("----------------- Stats ---------------------------------\n");
    printf("cumulative receive: %u (approx %u × %u = %u)\n",
           (unsigned)cumulate,
           (unsigned)N_SENDERS,
           (unsigned)strlen(TEST_STRING),
           (unsigned)(N_SENDERS * strlen(TEST_STRING)));
    printf("---------------------------------------------------------\n");

    nodeid_free(self);
}

void run_sender (int i)
{
    struct nodeID *self;
    struct nodeID *receiver;
    const uint8_t *buffer;
    ssize_t buflen;
    int retry;

    self = net_helper_init("127.0.0.1", START_PORT + i, "tcp_backlog=1");
    receiver = create_node("127.0.0.1", START_PORT);

    fprintf(stderr, "Sender %i waiting 0.75 sec...\n", i);
    usleep(750000);

    fprintf(stderr, "Sender %i starting transmission.\n", i);

    buffer = TEST_STRING;
    buflen = strlen(TEST_STRING);

    retry = MAX_RETRY;
    while (buflen > 0 && retry) {
        ssize_t N;

        N = send_to_peer(self, receiver, buffer, READ_BUFSIZE);
        if (N < 0) {
            fprintf(stderr, "Sender %i send anomalous, N=%i\n", i, (int)N);
            retry --;
            if (retry == 0) {
                fprintf(stderr, "Sender %i giving up.\n", i);
            }
        } else {
            buffer += N;
            buflen -= N;
        }
    }

    nodeid_free(receiver);
    nodeid_free(self);
}

int main (int argc, char **argv)
{
    pid_t peers[N_SENDERS + 1];
    int i;

    for (i = 1; i <= N_SENDERS; i ++) {
        pid_t P = fork();

        if (P == 0) {
            run_sender(i);
            exit(EXIT_SUCCESS);
        }
        peers[i] = P;
    }
    if ((peers[0] = fork()) == 0) {
        run_receiver();
        exit(EXIT_SUCCESS);
    }

    for (i = 0; i <= N_SENDERS; i ++) {
        int status;

        waitpid(peers[i], &status, 0);
        printf("Peer %i exited with status %i\n", i,
               WEXITSTATUS(status));
    }

    exit(EXIT_SUCCESS);
}

