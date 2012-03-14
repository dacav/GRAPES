#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <assert.h>
#include <string.h>

#include "net_helper.h"

static const uint16_t START_PORT = 9000;
static const char TEST_STRING[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Donec a diam"
    "lectus.\0 Sed sit amet ipsum mauris. Maecenas congue ligula ac quam"
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

#define NPEERS 5
static const size_t READ_BUFSIZE = 10;
static const int MAX_RETRY = 5;

typedef struct {
    struct nodeID * peers[NPEERS];
    struct nodeID * self;
    struct nodeID * turn_receiver;

    char *buffers[NPEERS];
} peer_data_t;

static peer_data_t * init (int id)
{
    int i, j;
    peer_data_t * ret = malloc(sizeof(peer_data_t));
    assert(ret != NULL);

    for (j = i = 0; i <= NPEERS; i ++) {
        uint16_t port = START_PORT + i;

        if (i == id) {
            fprintf(stderr, "Server for node, id=%i, port=%hu\n", id, port);
            ret->self = net_helper_init("127.0.0.1", port, NULL);
        } else {
            fprintf(stderr, "Target connection for node %i at port %hu\n",
                    id, port);
            ret->peers[j] = create_node("127.0.0.1", port);
            ret->buffers[j] = calloc(strlen(TEST_STRING) + 1,
                                     sizeof(char));
            j ++;
        }
    }
    ret->turn_receiver = NULL;

    return ret;
}

static void cleanup (peer_data_t *d)
{
    int i;

    for (i = 0; i < NPEERS; i ++) {
        nodeid_free(d->peers[i]);
    }
    nodeid_free(d->self);
}

#if 0
static void do_stats (struct nodeID *self, uint32_t *stats)
{
    char buffer[READ_BUFSIZE];
    int retry = MAX_RETRY;

    while (retry) {
        struct timeval timeout = {
            .tv_sec = 0,
            .tv_usec = 500000
        };

        fprintf(stderr, "Going in wait [retry=%i]...\n", retry);
        if (wait4data(self, &timeout, NULL) == 0) {
            fprintf(stderr, "Timeout [retry=%i]...\n", retry);
            retry --;
        } else {
            struct nodeID *remote;
            ssize_t N;

            fprintf(stderr, "Woken up. Now going in recv\n");
            N = recv_from_peer(self, &remote, buffer, READ_BUFSIZE);
            fprintf(stderr, "Received %i bytes from %s [retry=%i]\n",
                    (int)N, node_addr(remote), retry);
            if (N <= 0) {
                retry = 0;
            }
            nodeid_free(remote);
        }
    }

    fprintf(stderr, "End of experiment (retry was %i)\n", retry);
}

static void send_lorem_ipsum (struct nodeID *self, struct nodeID *target)
{
    int running = 1;

    const char *text = TEST_STRING;
    ssize_t len = strlen(TEST_STRING) + 1;

    fprintf(stderr, "Sender starting to send...\n");
    while (running) {
        size_t N;

        switch (N = send_to_peer(self, target, text, len)) {
            case -1:
                fprintf(stderr, "Error in sending\n");
            case 0:
                running = 0;
                break;
            default:
                text += N;
                len -= N;
                if (len <= 0) {
                    running = 0;
                }
                break;
        }
    }
}
#endif

static int sender_sync (peer_data_t *data)
{
    struct nodeID *remote;
    uint8_t msg;

    fprintf(stderr, "Sender waiting for receiver sync\n");
    if (recv_from_peer(data->self, &remote, (void *)&msg, 1) == -1) {
        return -1;
    }

    if (data->turn_receiver != NULL) {
        nodeid_free(data->turn_receiver);
    }
    data->turn_receiver = remote;

    fprintf(stderr, "Sender completing sync with %s\n", node_addr(remote));
    if (send_to_peer(data->self, remote, (void *)&msg, 1) == -1) {
        return -1;
    }
    return 0;
}

static int receiver_sync (peer_data_t *data)
{
    int i;
    uint8_t msg = 1;
    struct nodeID *remote;

    fprintf(stderr, "\n\nTurn will start in a second\n\n");
    sleep(1);   // Dirty way of giving other time to startup the server.
    for (i = 0; i < NPEERS; i ++) {

        fprintf(stderr, "Receiver, sync with %s\n",
                node_addr(data->peers[i]));
        if (send_to_peer(data->self, data->peers[i], (void *)&msg, 1) == -1) {
            return -1;
        }
    }

    while (i > 0) {
        fprintf(stderr, "Sync, %i missing...\n", i);
        if (recv_from_peer(data->self, &remote, (void *)&msg, 1) == -1) {
            return -1;
        }
        i --;
    }
    fprintf(stderr, "Receiver sync completed\n");

    return 0;
}

static void run_peer (int id)
{
    peer_data_t *data = init(id);
    int turn;

    for (turn = 0; turn <= NPEERS; turn ++) {
        if (turn == id) {
            if (receiver_sync(data) == -1) {
                fprintf(stderr, "Receiver cannot sync, interrupting\n");
            }
        } else {
            if (sender_sync(data) == -1) {
                fprintf(stderr, "Sender cannot sync, interrupting\n");
            }
        }
    }

    cleanup(data);
}

int main (int argc, char **argv)
{
    pid_t peers[NPEERS + 1];
    int i;

    for (i = 0; i <= NPEERS; i ++) {
        pid_t P = fork();

        if (P == 0) {
            run_peer(i);
            exit(EXIT_SUCCESS);
        }
        peers[i] = P;
    }

    for (i = 0; i <= NPEERS; i ++) {
        int status;

        waitpid(peers[i], &status, 0);
        printf("Peer %i exited with status %i\n", i,
               WEXITSTATUS(status));
    }

    exit(EXIT_SUCCESS);
}

