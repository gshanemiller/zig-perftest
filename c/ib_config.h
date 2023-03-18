#pragma once

#include <ib_common.h>

static const uint64_t HUGEPAGE_ALIGN_2MB = 0x200000;

struct UserParam {
  uint8_t   deviceId[64];
  uint8_t   clientMac[64];
  uint8_t   clientIp[64];
  uint32_t  clientPort;
  uint8_t   serverMac[64];
  uint8_t   serverIp[64];
  uint32_t  serverPort;
  uint32_t  iters;
  uint32_t  txQueueSize;
  uint32_t  rxQueueSize;
  uint32_t  payloadSize;
  uint32_t  portId;             // (some NICs are dual port. one-based)
  uint8_t   useHugePages;
  uint8_t   isServer;
};

struct SessionParam {
  const struct UserParam    *userParam;             // not owned
  const struct ibv_context  *context;               // not owned

  struct ibv_qp             *qp;
  struct ibv_qp_ex          *qpExt;
  struct mlx5dv_qp_ex       *devQp;
  struct ibv_mr             *mr;
  struct ibv_sge            *sge;
  struct ibv_cq             *send_cq;
  struct ibv_cq             *recv_cq;

  struct ibv_pd             *pd;                    // memory protection domain
  uint32_t                  shmid;                  // for huge pages
  void                      *hugePageMemory;        // pointer to allocated memory
  uint32_t                  hugePageAlignSizeBytes; // to help calc memory alignment
  uint64_t                  needHugePageSizeBytes;  // how much needed
  uint64_t                  hugePageSizeBytes;      // actual amount allocated w/ alignment

  uint32_t                  dctn;
  uint32_t                  dciStreamId;
};

// Return 0 if 'portId' in device 'context' is capable of send/receving ethernet packets and non-zero otherwise.
// Behavior is defined provided 'portId>0'.
int ice_ib_config_check_port_device(const struct ibv_context *context, int portId);

int ice_ib_allocate_session(const struct UserParam *param, const struct ibv_context *context, struct SessionParam *session); 
int ice_ib_deallocate_session(struct SessionParam *session); 

int ice_ib_set_mtu(const struct ibv_context *context, int mtuSizeBytes, int portId);
