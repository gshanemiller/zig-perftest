#pragma once

#include <ib_common.h>

static const uint64_t HUGEPAGE_ALIGN_2MB = 0x200000;
static const uint64_t CPU_CACHE_LINE_SIZE_BYTES = 64;

struct UserParam {
  char      deviceId[64];
  char      clientMac[64];
  char      serverMac[64];
  char      clientIpAddr[64];
  char      serverIpAddr[64];
  uint16_t  clientPort;
  uint16_t  serverPort;
  uint32_t  iters;
  uint32_t  txQueueSize;
  uint32_t  rxQueueSize;
  uint16_t  payloadSize;
  uint32_t  portId;             // (some NICs are dual port. one-based)
  uint8_t   useHugePages;
  uint8_t   isServer;
};

struct SessionParam {
  const struct UserParam    *userParam;             // not owned
  const struct ibv_context  *context;               // not owned

  uint8_t                   serverMac[6];           // server MAC address in network binary format
  uint8_t                   clientMac[6];           // server MAC address in network binary format
  uint32_t                  serverIpAddr;           // server IPV4 address (192.16.0.2) in network binary format
  uint32_t                  clientIpAddr;           // server IPV4 address (192.16.0.2) in network binary format
  uint16_t                  serverPort;             // server IPV4 port in network binary format
  uint16_t                  clientPort;             // client IPV4 port in network binary format

  struct ibv_qp             *qp;
  struct ibv_qp_ex          *qpExt;
  struct mlx5dv_qp_ex       *devQp;
  struct ibv_mr             *mr;
  struct ibv_sge            *sge;
  struct ibv_cq             *send_cq;
  struct ibv_cq             *recv_cq;
  struct ibv_pd             *pd;                    // memory protection domain

https://www.rdmamojo.com/2012/05/05/qp-state-machine/
https://www.cs.mtsu.edu/~waderholdt/6430/papers/ibverbs.pdf
struct ibv_comp_channel *ibv_create_comp_channel(struct ibv_context *context);
struct ibv_comp_channel *ibv_create_comp_channel(struct ibv_context *context);
hannel = ibv_create_comp_channel(context);
if (!channel) {
fprintf(stderr, "Error, ibv_create_comp_channel() failed\n");
return -1;
}
if (ibv_destroy_comp_channel(channel)) {
fprintf(stderr, "Error, ibv_destroy_comp_channel() failed\n");
return -1;
}

  void                      *hugePageMemory;        // pointer to allocated memory
  uint32_t                  hugePageAlignSizeBytes; // to help calc memory alignment
  uint64_t                  needHugePageSizeBytes;  // how much needed
  uint64_t                  hugePageSizeBytes;      // actual amount allocated w/ alignment
  uint32_t                  shmid;                  // for huge pages
  uint8_t                   *currentPacket;         // current packet (see ice_ib_make_ipv4packet)
  uint8_t                   *nextPacket;            // where next packet will be created (see ice_ib_make_ipv4packet);

  uint32_t                  dctn;
  uint32_t                  dciStreamId;
};

// Return 0 if 'portId' in device 'context' is capable of send/receving ethernet packets and non-zero otherwise.
// Behavior is defined provided 'portId>0'.
int ice_ib_config_check_port_device(const struct ibv_context *context, int portId);

int ice_ib_allocate_session(const struct UserParam *param, const struct ibv_context *context, struct SessionParam *session); 
int ice_ib_deallocate_session(struct SessionParam *session); 

int ice_ib_set_rtr(struct SessionParam *session);
int ice_ib_set_rts(struct SessionParam *session);
