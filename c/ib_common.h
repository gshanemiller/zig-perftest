#pragma once

#include <ib.h>
#include <umad.h>
#include <verbs.h>
#include <mlx5_api.h>

#include <assert.h>

// define to not conflict with errno
enum ICE_IB_Error {
  ICE_IB_ERROR_NO_DEVICE = -1,
  ICE_IB_ERROR_MAX = -2,
};
