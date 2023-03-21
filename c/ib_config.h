#pragma once

#include <ib.h>
#include <umad.h>
#include <verbs.h>
#include <mlx5dv.h>
#include <mlx5_api.h>

static const uint64_t HUGEPAGE_ALIGN_2MB = 0x200000;
static const uint64_t CPU_CACHE_LINE_SIZE_BYTES = 64;
static const uint64_t CPU_CACHE_LINE_SIZE_MASK = (64-1);

enum kMAX {
  MAC_ADDR_SIZE = 6,
  MAX_PACKET_BURST_SIZE = 2,
  MAX_SEND_QUEUE_ENTRIES = 128,
  MAX_RECV_QUEUE_ENTRIES = 128,
  MAX_COMPLETION_QUEUE_ENTRIES = 128
};

// define to not conflict with errno
enum ICE_IB_Error {
  ICE_IB_ERROR_NO_DEVICE = -1,        // Zero IB devices found
  ICE_IB_ERROR_ENOENT_DEVICE = -2,    // Requested IB device not known
  ICE_IB_ERROR_NO_MEMORY = -3,        // Memory allocation failed
  ICE_IB_ERROR_BAD_IP_ADDR = -4,      // Bad textual MAC or IPV4 IP address 
  ICE_IB_ERROR_API_ERROR = -5,        // ib/mlx5 api error not enumerated elsewhere
  ICE_IB_ERROR_MAX = -6,
};

#pragma pack(push,1)
struct IPHeader {
  uint8_t     dstMac[6];
  uint8_t     srcMac[6];
  uint16_t    ethType;
};

struct IPV4Header {
  uint8_t     ihl:4;
  uint8_t     version:4;
  uint8_t     typeOfService;
  uint16_t    size;
  uint16_t    packetId;
  uint16_t    fragmentOffset;
  uint8_t     ttl;
  uint8_t     nextProtoId;
  uint16_t    checksum;
  uint32_t    srcIpAddr;
  uint32_t    dstIpAddr;
};

struct IPV4UDPHeader {
  uint16_t    srcPort;
  uint16_t    dstPort;
  uint16_t    size;
  uint16_t    checksum;
};

// Assumes little endian callers
struct IPV4Packet {
  struct IPHeader       ip_header;
  struct IPV4Header     ipv4_header;
  struct IPV4UDPHeader  ipv4udp_header;
  char payload[];
};
#pragma pack(pop)

struct IPV4UDPEndpoint {
  uint32_t                  ipAddr;                           // IPV4 address (192.16.0.2) in network binary format
  uint16_t                  port;                             // server IPV4 port in network binary format
  uint8_t                   mac[MAC_ADDR_SIZE];               // MAC address in network binary format
};

struct UserParam {
  char                      deviceId[64];
  char                      clientMac[64];
  char                      serverMac[64];
  char                      clientIpAddr[64];
  char                      serverIpAddr[64];
  uint16_t                  clientPort;
  uint16_t                  serverPort;
  uint32_t                  iters;
  uint16_t                  payloadSize;
  uint32_t                  portId;                           // some NICs are dual port. one-based
  uint8_t                   useHugePages;
  uint8_t                   isServer;
};

struct HugePageMemory {
  const void                *hugePageMemory;                  // pointer to allocated memory
  uint64_t                  requestSizeBytes;                 // how much requested
  uint64_t                  actualSizeBytes;                  // actual alloc rounded up to nearest HUGEPAGE_ALIGN_2MB
  uint32_t                  shmid;                            // shared memory handle to huge memory
};

struct Queue {
  const uint8_t             *start;                           // start of packet memory
  const uint8_t             *end;                             // end of packet memory after which wrap around to start
  struct ibv_mr             *mr;                              // memory registration [start, end)
  struct ibv_cq             *cq;                              // completion queue
  struct ibv_sge            sqe[MAX_PACKET_BURST_SIZE];       // scatter-gather memory (to send or receive into)
  union {
    struct ibv_send_wr      wsq[MAX_SEND_QUEUE_ENTRIES];      // work request queue (for senders)
    struct ibv_recv_wr      wrq[MAX_RECV_QUEUE_ENTRIES];      // work request queue (for receivers)
  };
  struct IPV4Packet         *packet;                          // TMP
};

struct SessionCommon {
  struct ibv_qp             *qp;                              // queue pair coordinating send/recv members
  struct ibv_pd             *pd;                              // memory protection domain (for multiple memory regions)
  struct ibv_context        *context;                         // NIC device context
  struct ibv_device         *device;                          // the device session operates on
  struct ibv_device         **deviceList;                     // all known devices at initialize time
};

struct Session {
  struct Queue              *send;                            // conveneince pointer into 'sendMemory' memory
  struct Queue              *recv;                            // conveneince pointer into 'recvMemory' memory
  struct SessionCommon      *common;                          // conveneince pointer into 'cmmnMemory' memory

  struct HugePageMemory     sendMemory;                       // huge page memory for send work
  struct HugePageMemory     recvMemory;                       // huge page memory for receive work
  struct HugePageMemory     cmmnMemory;                       // huge page memory for data common to send, recv

  struct IPV4UDPEndpoint    server;                           // server endpoint in binary network order
  struct IPV4UDPEndpoint    client;                           // client endpoint in binary network order

  const struct UserParam    *userParam;                       // not owned
};

int ice_verb_config_check_port_device(struct ibv_context *context, int portId);
struct ibv_device **ice_verb_find_device(const char *deviceName, struct ibv_device **device, int *rc);

int ice_verb_allocate_huge_memory(uint64_t requestSizeBytes, struct HugePageMemory *memory);

int ice_verb_initialize_queue(struct ibv_pd *pd, struct ibv_context *context, struct HugePageMemory *memory);
int ice_verb_deinitialize_queue(struct Queue *queue);

int ice_verb_initialize_session_common(const struct UserParam *param, struct Queue *send, struct Queue *recv,
  struct ibv_context *context, struct ibv_pd *pd, struct ibv_device *device, struct ibv_device **deviceList,
  struct HugePageMemory *memory);
int ice_verb_deinitalize_session_common(struct SessionCommon *common);

int ice_verb_allocate_session(const struct UserParam *param, struct Session *session);
int ice_verb_deallocate_session(struct Session *session); 

int ice_verb_initialize_endpoint(const char *mac, const char *ipAddr, uint16_t port, struct IPV4UDPEndpoint *endpoint);

int ice_verb_make_raw_ipv4packet(struct Queue *queue, struct IPV4UDPEndpoint *src, struct IPV4UDPEndpoint *dst,
  uint16_t payloadSizeBytes);
int ice_verb_checksum_ipv4packet(struct IPV4Packet *packet);

int ice_verb_set_rtr(struct Session *session);
int ice_verb_set_rts(struct Session *session);
