#include <stdio.h>
#include <ib_device.h>

int main() {
  struct ibv_device *device = 0;
  struct ibv_context *context = 0;
  if (0==ice_ib_find_device("rocep1s0f1", &device)) {
    ice_ib_open_context(device, &context);
    ice_ib_close_context(context);
  }
  return 0;
}
