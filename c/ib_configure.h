#pragma

#include <ib_common.h>

struct Param {
  char  deviceId[64];
  char  clientMac[64];
  char  clientIp[64];
  int   clientPort;
  char  serverMac[64];
  char  serverIp[64];
  int   serverPort;
  int   iters;
  int   txQueueSize;
  int   rxQueueSize;
  int   payloadSize;
  int   ibPort;             // (some NICs are dual port)
  char  useHugePages;
  char  isServer;
};

// Return 0 if 
ib_config_init_link_layer(const struct ibv_context *context, const struct Param *param);
