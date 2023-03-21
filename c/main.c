#include <ib_config.h>

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
  param.iters = 1;
  param.payloadSize = 128;
  param.portId = 1;
  param.useHugePages = 1;
  param.isServer = 0;

  struct Session session;
  if (0!=(rc=ice_verb_allocate_session(&param, &session))) {
    ice_verb_set_rtr(&session);
    ice_verb_set_rts(&session);
  }

  // Free whatever was allocated
  ice_verb_deallocate_session(&session);

  return rc;
}
