#pragma once
#include <memory>

#include "messages.h"

namespace ipc {

struct InternalEnvRequestPayload {
  float x;
  float y;
};

struct InternalEnvDataPayload {
  std::shared_ptr<const EnvironmentPayload> ptr;
};

}  // namespace ipc
