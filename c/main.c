#include <ib_device.h>
#include <ib_config.h>

int main() {
  int rc;
  struct UserParam param = {0};
  struct SessionParam session;
  struct ibv_device *device = 0;
  struct ibv_context *context = 0;

  param.portId = 1;
  param.useHugePages = 1;
  param.txQueueSize = 256;
  param.rxQueueSize = 256;

  rc = ice_ib_find_device("rocep1s0f1", &device);

  if (0==rc) {
    rc = ice_ib_open_context(device, &context);
    rc = ice_ib_config_check_port_device(context, param.portId);
    rc = ice_ib_allocate_session(&param, context, &session);
    rc = ice_ib_set_rtr(&session);
    rc = ice_ib_set_rts(&session);
    rc = ice_ib_deallocate_session(&session);
    rc = ice_ib_close_context(context);
  }

  return rc==0;
}
