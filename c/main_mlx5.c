#include <ice_mlx5.h>

int main() {
  int rc;
  struct UserParam param = {0};

  strcpy(param.deviceId, "rocep1s0f1");
  strcpy(param.clientMac, "08:c0:eb:d4:d0:df");
  strcpy(param.serverMac, "08:c0:eb:d4:d0:df");
  strcpy(param.clientIpAddr, "192.168.0.2");
  strcpy(param.serverIpAddr, "192.168.0.2");
  param.clientPort = 10011;
  param.serverPort = 10013;
  param.iters = 100;
  param.portId = 1;
  param.isServer = 0;

  // Initialize session
  struct Session session;
  ice_mlx5_allocate_session(&param, &session);

  // Free whatever was allocated
  ice_mlx5_deallocate_session(&session);

  return rc;
}
