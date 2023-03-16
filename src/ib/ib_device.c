#include <ib_device.h>
#include <string.h>
#include <stdio.h>

int ice_ib_find_device(const char *deviceName, struct ibv_device **device) {
  assert(deviceName);
  assert(strlen(deviceName)>0);
  assert(device);

  // Initialize
  *device = 0;

  // Request device list and iterate until found
  int deviceCount = -1;
  struct ibv_device **deviceList = ibv_get_device_list(&deviceCount);

  printf("got %p %d\n", deviceList, deviceCount);
  
  if (deviceCount<=0) {
    return (ICE_IB_ERROR_NO_DEVICE);
  }

  return 0;
}
