#include <ib_device.h>
#include <string.h>
#include <stdio.h>

int ice_ib_find_device() {
  int deviceCount = -1;
  ibv_get_device_list(&deviceCount);
  printf("got %d devices expect (2) devices\n", deviceCount);
  return 0;
}
