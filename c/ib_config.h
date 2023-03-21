#pragma once

#include <ib_common.h>

statoc const uint32_t MAC_ADDR_SIZE = 6;
static const uint32_t MAX_PACKET_BURST_SIZE = 2;
static const uint64_t HUGEPAGE_ALIGN_2MB = 0x200000;
static const uint64_t CPU_CACHE_LINE_SIZE_BYTES = 64;

struct UserParam {
  char                      deviceId[64];
  char                      clientMac[64];
  char                      serverMac[64];
  char                      clientIpAddr[64];
  char                      serverIpAddr[64];
  uint16_t                  clientPort;
  uint16_t                  serverPort;
  uint32_t                  iters;
  uint32_t                  txQueueSize;
  uint32_t                  rxQueueSize;
  uint16_t                  payloadSize;
  uint32_t                  portId;                           // some NICs are dual port. one-based
  uint8_t                   useHugePages;
  uint8_t                   isServer;
};

struct HugePageMemory {
  const void                *hugePageMemory;                  // pointer to allocated memory
  uint64_t                  requestSizeBytes;                 // how much needed
  uint64_t                  actualSizeBytes;                  // actual amount allocated
  uint32_t                  shmid;                            // shared memory handle to huge memory
};

struct IPV4UDPQueue {
  const uint8_t             *start;                           // start of packet memory
  const uint8_t             *end;                             // end of packet memory after which wrap around to start
  struct ibv_pd             *pd;                              // memory protection domain
  struct ibv_mr             *mr;                              // memory registration of pd
  struct ibv_cq             *cq;                              // completion queue
  struct ibv_send_wr        wq;                               // work request queue (for senders)
  struct ibv_recv_rr        rq;                               // work request queue (for receivers)
  struct ibv_sge            sqe[MAX_PACKET_BURST_SIZE];       // scatter-gather memory (to send or receive into)
};

struct SessionCommon {
  struct ibv_qp             *qp;                              // queue pair coordinating send/recv members

  uint32_t                  srcIpAddr;                        // IPV4 address (192.16.0.2) in network binary format
  uint16_t                  srcPort;                          // server IPV4 port in network binary format
  uint8_t                   srcMac[MAC_ADDR_SIZE];            // MAC address in network binary format

  uint32_t                  dstIpAddr;                        // IPV4 address (192.16.0.2) in network binary format
  uint16_t                  dstPort;                          // server IPV4 port in network binary format
  uint8_t                   dstMac[MAC_ADDR_SIZE];            // MAC address in network binary format
};

struct Session {
  struct IPV4UDPQueue       *send;                            // conveneince pointer into huge memory in 'sendMemory'
  struct IPV4UDPQueue       *recv;                            // conveneince pointer into huge memory in 'recvMemory'
  struct SessionCommon      *common;                          // conveneince pointer into huge memory in 'cmmnMemory'

  struct HugePageMemory     sendMemory;                       // huge page memory for send work
  struct HugePageMemory     recvMemory;                       // huge page memory for receive work
  struct HugePageMemory     cmmnMemory;                       // huge page memory for data common to send, recv

  const struct UserParam    *userParam;                       // not owned
  const struct ibv_context  *context;                         // not owned
};

// Return 0 if 'portId' in device 'context' is capable of send/receving ethernet packets and non-zero otherwise.
// Behavior is defined provided 'portId>0'.
int ice_ib_config_check_port_device(const struct ibv_context *context, int portId);

int ice_ib_allocate_huge_memory(uint64_t requestSizeBytes, struct HugePageMemory *memory);

int ice_ib_initialize_ipv4_udp_queue(const struct UserParam *param, struct ibv_context *context, 
  const HugePageMemory *memory);
int ice_ib_deinitialize_ipv4_udp_queue(HugePageMemory *memory);

int ice_ib_initialize_session_common(struct SessionCommon *common);
int ice_ib_deinitalizedsession_common(struct SessionCommon *common);

int ice_ib_allocate_session(const struct UserParam *param, struct ibv_context *context, struct Session *session); 
int ice_ib_deallocate_session(struct Session *session); 

int ice_ib_set_rtr(struct Session *session);
int ice_ib_set_rts(struct Session *session);
