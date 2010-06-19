/*
 *  Copyright (c) 2010 Marco Biazzini
 *
 *  This is free software; see lgpl-2.1.txt
 */

#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "net_helper.h"
#include "topmanager.h"
#include "topocache.h"
#include "topo_proto.h"
#include "proto.h"
#include "msg_types.h"
#include "tman.h"

#define TMAN_INIT_PEERS 20 // max # of neighbors in local cache (should be > than the next)
#define TMAN_MAX_PREFERRED_PEERS 10 // # of peers to choose a receiver among (should be < than the previous)
#define TMAN_MAX_GOSSIPING_PEERS 10 // # size of the view to be sent to receiver peer (should be <= than the previous)
#define TMAN_IDLE_TIME 10 // # of iterations to wait before switching to inactive state
#define TMAN_STD_PERIOD 3000000
#define TMAN_INIT_PERIOD 1000000

//static const int MAX_PEERS = TMAN_MAX_PEERS;
static  int MAX_PREFERRED_PEERS = TMAN_MAX_PREFERRED_PEERS;
static  int MAX_GOSSIPING_PEERS = TMAN_MAX_GOSSIPING_PEERS;
static  int IDLE_TIME = TMAN_IDLE_TIME;

static uint64_t currtime;
static int cache_size = TMAN_INIT_PEERS;
static struct peer_cache *local_cache = NULL;
static int period = TMAN_INIT_PERIOD;
static int active = 0;
static int do_resize = 0;
static void *mymeta;

static tmanRankingFunction rankFunct;


// TODO: first parameter may be discarded, because it is always called with local_cache...
static struct peer_cache *rank_cache (const struct peer_cache *c, const struct nodeID *target, const void *target_meta)
{
	struct peer_cache *res;
	int i, msize;
	const uint8_t *mdata;

        mdata = get_metadata(c,&msize);
	res = cache_init(cache_size,msize);
        if (res == NULL) {
          return res;
        }

        for (i=0; nodeid(c,i); i++) {
		if (!nodeid_equal(nodeid(c,i),target))
			cache_add_ranked(res,nodeid(c,i),mdata+i*msize,msize, rankFunct, target_meta);
	}

	return res;
}


static uint64_t gettime(void)
{
  struct timeval tv;

  gettimeofday(&tv, NULL);

  return tv.tv_usec + tv.tv_sec * 1000000ull;
}

int tmanInit(struct nodeID *myID, void *metadata, int metadata_size, ranking_function rfun, int gossip_peers)
{
  rankFunct = rfun;
  topo_proto_init(myID, metadata, metadata_size);
  mymeta = metadata;
  
  local_cache = cache_init(cache_size, metadata_size);
  if (local_cache == NULL) {
    return -1;
  }
  IDLE_TIME = TMAN_IDLE_TIME;
  if (gossip_peers) {
    MAX_GOSSIPING_PEERS = gossip_peers;
  }
  MAX_PREFERRED_PEERS = TMAN_MAX_PREFERRED_PEERS;
  active = IDLE_TIME;
  currtime = gettime();

  return 0;
}

int tmanGivePeers (int n, struct nodeID **peers, void *metadata)
{
	int metadata_size;
	const uint8_t *mdata;
	int i;

        mdata = get_metadata(local_cache, &metadata_size);
	for (i=0; nodeid(local_cache, i) && (i < n); i++) {
			peers[i] = nodeid(local_cache,i);
			if (metadata_size)
				memcpy((uint8_t *)metadata + i * metadata_size, mdata + i * metadata_size, metadata_size);
	}
	if (i != n) {
		active = 0;
        }

	return i;
}

int tmanGetNeighbourhoodSize(void)
{
  int i;

  for (i = 0; nodeid(local_cache, i); i++);

  return i;
}

static int time_to_send(void)
{
	if (gettime() - currtime > period) {
		currtime += period;
		if (active > 0)
			return 1;
	}

  return 0;
}

int tmanAddNeighbour(struct nodeID *neighbour, void *metadata, int metadata_size)
{
  if (cache_add_ranked(local_cache, neighbour, metadata, metadata_size, rankFunct, mymeta) < 0) {
    return -1;
  }

  return 1;//topo_query_peer(local_cache, neighbour, TMAN_QUERY);
}


// not self metadata, but neighbors'.
const void *tmanGetMetadata(int *metadata_size)
{
  return get_metadata(local_cache, metadata_size);
}


int tmanChangeMetadata(struct nodeID *peer, void *metadata, int metadata_size)
{
  if (topo_proto_metadata_update(peer, metadata, metadata_size) <= 0) {
    return -1;
  }
  mymeta = metadata;

  return 1;
}


int tmanParseData(const uint8_t *buff, int len, const struct nodeID **peers, int size, const void *metadata, int metadata_size)
{
        int msize,s;
        const uint8_t *mdata;
	struct peer_cache *new;
	int source;

	if (len) {
		const struct topo_header *h = (const struct topo_header *)buff;
		struct peer_cache *remote_cache; int idx;

	    if (h->protocol != MSG_TYPE_TMAN) {
	      fprintf(stderr, "TMAN: Wrong protocol!\n");
	      return -1;
	    }

	    if (h->type != TMAN_QUERY && h->type != TMAN_REPLY) {
	      return -1;
	    }
		remote_cache = entries_undump(buff + sizeof(struct topo_header), len - sizeof(struct topo_header));
		mdata = get_metadata(remote_cache,&msize);
		get_metadata(local_cache,&s);

		if (msize != s) {
			fprintf(stderr, "TMAN: Metadata size mismatch! -> local (%d) != received (%d)\n",
				s, msize);
			return 1;
		}

		if (h->type == TMAN_QUERY) {
//			fprintf(stderr, "\tTman: Parsing received msg to reply...\n");
			new = rank_cache(local_cache, nodeid(remote_cache, 0), get_metadata(remote_cache, &msize));
			if (new) {
				tman_reply(remote_cache, new);
				cache_free(new);
				// TODO: put sender in tabu list (check list size, etc.), if any...
			}
		}
		idx = cache_add_ranked(local_cache, nodeid(remote_cache,0), mdata, msize, rankFunct, mymeta);
//		fprintf(stderr, "\tidx = %d\n",idx);
		new = merge_caches_ranked(local_cache, remote_cache, cache_size, &source, rankFunct, mymeta);

//		  newsize = new_cache->current_size>c1->cache_size?new_cache->current_size:c1->cache_size;
//		  newsize = cache_shrink(new_cache,(new_cache->cache_size-newsize));
//		  // TODO: check newsize??
//		  return new_cache;

		cache_free(remote_cache);
		if (new!=NULL) {
		  cache_free(local_cache);
		  local_cache = new;
                }
                if (source) {
		  if (idx>=0 || source!=1) {
                  	active = IDLE_TIME;
                  }
                  else {
                  	period = TMAN_STD_PERIOD;
                  	if (active>0) active--;
                  }

                  do_resize = 0;
		}
	}

  if (time_to_send()) {
	uint8_t *meta;
	struct nodeID *chosen;

	cache_update(local_cache);
	if (active == 0) {
		struct peer_cache *ncache;
		int j, res = 0;

		ncache = cache_init(size,metadata_size);
		for (j=0;j<size && res!=-3;j++)
			res = cache_add_ranked(ncache, peers[j],(const uint8_t *)metadata + j * metadata_size, metadata_size, rankFunct, mymeta);
		if (nodeid(ncache, 0)) {
			new = merge_caches_ranked(local_cache, ncache, cache_size, &source, rankFunct, mymeta);
                        if (new) {
				cache_free(local_cache);
				local_cache = new;
			}
			if (source!=1) {
				active = TMAN_IDLE_TIME;
			}
			do_resize = 0;
		}
		cache_free(ncache);
	}
		
	mdata = get_metadata(local_cache,&msize);
	chosen = rand_peer(local_cache, &meta);		//MAX_PREFERRED_PEERS
	new = rank_cache(local_cache, chosen, meta);
	if (new==NULL) {
		fprintf(stderr, "TMAN: No cache could be sent to remote peer!\n");
		return 1;
	}
	tman_query_peer(new, chosen);
	cache_free(new);
  }

  return 0;
}



// limit : at most it doubles the current cache size...
int tmanGrowNeighbourhood(int n)
{
	if (n<=0 || do_resize)
		return -1;
	n = n>cache_size?cache_size:n;
	cache_size += n;
	do_resize = 1;
	return cache_size;
}


int tmanShrinkNeighbourhood(int n)
{
	if (n<=0 || n>=cache_size || do_resize)
		return -1;
	cache_size -= n;
	do_resize = 1;
	return cache_size;
}

