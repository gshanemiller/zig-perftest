#include <ib_device.h>

int ice_ib_find_device(const char *deviceName, struct ibv_device **device) {
  assert(deviceName);
  assert(strlen(deviceName)>0);
  assert(device);

  // Initialize
  *device = 0;
  int rc = ICE_IB_ERROR_NO_DEVICE;

  // Get device list and count
  int deviceCount = -1;
  struct ibv_device **list = ibv_get_device_list(&deviceCount);

  if (deviceCount<=0 || list==0) {
    int rc = errno;
    fprintf(stderr, "warn : ibv_get_device_list failed: %s (errno %d)\n", strerror(rc), rc);
    return (rc);
  }

  // See if 'deviceName' is found in list
  for (struct ibv_device **item = list; *item; ++item) {
    assert(*list);
    if (!strcmp(ibv_get_device_name(*item), deviceName)) {
      // matched device
      *device = *item;
      rc = 0;
      break;
    }
  }

  // cleanup memory
  ibv_free_device_list(list);

  return rc==0 ? 0 : ICE_IB_ERROR_ENOENT_DEVICE;
}

int ice_ib_open_context(const struct ibv_device *device, struct ibv_context **context) {
  assert(device);
  assert(context);

  *context = ibv_open_device((struct ibv_device *)device);

  if (*context==0) {
    int rc = errno;
    fprintf(stderr, "warn : ibv_open_device: failed: %s (errno %d)\n", strerror(rc), rc);
    return ICE_IB_ERROR_API_ERROR;
  }

  return 0;
}

int ice_ib_close_context(struct ibv_context *context) {
  assert(context);
  if (0!=ibv_close_device(context)) {
    int rc = errno;
    fprintf(stderr, "warn : ibv_close_device: failed: %s (errno %d)\n", strerror(rc), rc);
    return ICE_IB_ERROR_API_ERROR;
  }

  return 0;
}
