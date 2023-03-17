#pragma once

#include <ib_common.h>

// Return 0 if the Infiniband device 'deviceName' was found and the
// pointer handle '*device' is initialized to point to it and non-zero
// otherwise. Behavior is defined provided 'strlen(deviceName)>0'.
int ice_ib_find_device(const char *deviceName, struct ibv_device **device);

// Return 0 if the Infiniband 'device' was opened and the pointer
// handle 'context' is initialized to point to it and non-zero otherwise.
// Note that on success the context must be closed. see below
int ice_ib_open_context(const struct ibv_device *device, struct ibv_context **context);

// Return 0 if the 'context' was closed and non-zero otherwise. Note that
// upon return the memory pointed at by 'context' is undefined
int ice_ib_close_context(struct ibv_context *context);
