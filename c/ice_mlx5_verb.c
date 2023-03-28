#include <ice_mlx5.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>

struct ibv_device **ice_mlx5_find_device(const char *deviceName, struct ibv_device **device, int *rc) {
  assert(deviceName);
  assert(strlen(deviceName)>0);
  assert(device);
  assert(rc);

  // Initialize
  *device = 0;
  *rc = ICE_IB_ERROR_NO_DEVICE;

  // Get device list and count
  int deviceCount = -1;
  struct ibv_device **list = ibv_get_device_list(&deviceCount);

  if (deviceCount<=0 || list==0) {
    int rc = errno;
    fprintf(stderr, "warn : ice_mlx5_find_device.ibv_get_device_list failed: %s (errno %d)\n", strerror(rc), rc);
    return 0;
  }

  // See if 'deviceName' is found in list
  for (struct ibv_device **item = list; *item; ++item) {
    if (!strcmp(ibv_get_device_name(*item), deviceName)) {
      // matched device
      *device = *item;
      rc = 0;
      break;
    }
  }

  // OK there are devices but none matching 'deviceName'
  if (rc!=0) {
    *rc = ICE_IB_ERROR_NO_DEVICE;
  }

  return list;
}

int ice_mlx5_open_device(struct ibv_device *device, struct mlx5dv_context_attr *attrs, struct ibv_context **context) {
  assert(device);
  assert(attrs);
  assert(context);

  // Initialize
  *context = 0;
  memset(attrs, 0, sizeof(struct mlx5dv_context_attr));
  attrs->flags = MLX5DV_CONTEXT_FLAGS_DEVX;

  // Open a context on found device, if any
  *context = mlx5dv_open_device(device, attrs);
  if (*context==0) {
    int rc = errno;
    fprintf(stderr, "warn : ice_mlx5_open_device.mlx5dv_open_device failed: %s (errno %d)\n", strerror(rc), rc);
    return ICE_IB_ERROR_NO_DEVICE;
    }
  }

  return 0;
}

int ice_mlx5_query_devport(struct SessionCommon *common, int portId) {
  assert(common);
  assert(common->context);
  assert(common->device);
  assert(common->deviceList);
  assert(portId>0);

  if (0!=mlx5dv_query_device(common->context, &common->contextExtended)) {
    int rc = errno;
    fprintf(stderr, "warn : ice_mlx5_query_devport.mlx5dv_query_device %d: failed: %s (errno %d)\n", portId, strerror(rc), rc);                            
    return ICE_IB_ERROR_API_ERROR;
  }

  if (0!=ibv_query_port(common->context, portId, &common->portData)) {
    int rc = errno;
    fprintf(stderr, "warn : ice_mlx5_query_devport.ibv_query_port %d: failed: %s (errno %d)\n", portId, strerror(rc), rc);                            
    return ICE_IB_ERROR_API_ERROR;
  }

  if (0!=mlx5dv_query_port(common->context, portId, &common->portDataExtended)) {
    int rc = errno;
    fprintf(stderr, "warn : ice_mlx5_query_devport.mlx5dv_query_port %d: failed: %s (errno %d)\n", portId, strerror(rc), rc);                            
    return ICE_IB_ERROR_API_ERROR;
  }

  if (common->portData.state != IBV_PORT_ACTIVE) {
    fprintf(stderr, "warn : ice_mlx5_query_devport.ibv_query_port: port %d not active (state %d)\n", portId, common->portData.state);
    return ICE_IB_ERROR_NO_DEVICE;
  }

  if (common->portData.link_layer != IBV_LINK_LAYER_ETHERNET) {
    fprintf(stderr, "warn : ice_mlx5_query_devport.ibv_query_port: port %d not configured for ethernet. set %u\n", portId, common->portData.link_layer);
    return ICE_IB_ERROR_NO_DEVICE;
  }

  return 0;
}

int ice_mlx5_allocate_session(const struct UserParam *param, struct Session *session) {
  assert(param);
  assert(session);

  // Initialize session
  memset(session, 0, sizeof(struct Session));

  // See if the callers's device exists
  int rc;
  struct ibv_device *device;
  struct ibv_device **deviceList;
  deviceList=ice_mlx5_find_device(param->deviceId, &device, &rc);
  if (deviceList==0 || device==0 || rc!=0) {
      // No viable device
      return ICE_IB_ERROR_NO_DEVICE;
  }

  // Open a context on found device, if any
  struct ibv_context *context = 0;
  struct mlx5dv_context_attr contextAttrs = {0};
  if (0!=ice_mlx5_open_device(device, &contextAttrs, &context) {
    if (context) {
      ibv_close_device(context);
      context = 0;
    }
    if (deviceList) {
      ibv_free_device_list(deviceList);
      deviceList = 0;
    }
    // No viable device
    return ICE_IB_ERROR_NO_DEVICE;
  }

  // Start initializing session
  char valid = 1;
  session->userParam = param;
  session->context = context;
  session->common.device = device;
  session->common.deviceList = deviceList;

  // Prepare session endpoints in network ready binary format
  if (0!=ice_verb_initialize_endpoint(param->clientMac, param->clientIpAddr, param->clientPort, &session->client)) {
    valid = 0;
    return ICE_IB_ERROR_BAD_IP_ADDR;
  }
  if (0!=ice_verb_initialize_endpoint(param->serverMac, param->serverIpAddr, param->serverPort, &session->server)) {
    valid = 0;
    return ICE_IB_ERROR_BAD_IP_ADDR;
  }

  // Make sure NIC and its specified port in good state
  if (0!=(ice_mlx5_query_devport(&session->common, param->portId))) {
    valid = 0;
    // No viable device
    return ICE_IB_ERROR_NO_DEVICE;
  }

  return valid ? 0 : ICE_IB_ERROR_API_ERROR;
}

int ice_mlx5_deallocate_session(struct Session *session) {
  assert(session);

  if (session->common) {
    ice_verb_deinitalize_session_common(session->common);
  }

  memset(session, 0, sizeof(struct Session));

  return 0;
}

int ice_mlx5x_deinitalize_session_common(struct SessionCommon *common) {
  assert(common);

  if (common->qp) {
    ibv_destroy_qp(common->qp);
  }
  if (common->pd) {
    ibv_dealloc_pd(common->pd);
  }
  if (common->context) {
    ibv_close_device(common->context);
  }
  if (common->deviceList) {
    ibv_free_device_list(common->deviceList);
  }

  memset(common, 0, sizeof(struct SessionCommon));

  return 0;
}

int ice_mlx5_initialize_endpoint(const char *mac, const char *ipAddr, uint16_t port, struct IPV4UDPEndpoint *endpoint) {
  char valid = 1;

  // Get IPV4 address into network ready binary format
  if (0==(inet_pton(AF_INET, ipAddr, &endpoint->ipAddr))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_verb_initialize_endpoint: inet_pton failed on '%s': %s (errno %d)\n",
      ipAddr, strerror(rc), rc);
    valid = 0;
  }

  // Get IPV4 UDP port into network ready binary format
  endpoint->port = htons(port);

  // Get IP MAC addreses into network ready binary format
  uint32_t macData[6];
  if (MAC_ADDR_SIZE!=(sscanf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
    macData+0, macData+1, macData+2,
    macData+3, macData+4, macData+5))) {
    int rc = errno;
    fprintf(stderr, "warn : ice_verb_initialize_endpoint: MAC address invalid '%s': %s (errno %d)\n",
      mac, strerror(rc), rc);
    valid = 0;
  }
  for (uint16_t i=0; i<MAC_ADDR_SIZE; ++i) {
    endpoint->mac[i] = (uint8_t)(macData[i] & 0xff);
  }

  return valid ? 0 : ICE_IB_ERROR_BAD_IP_ADDR;
}
