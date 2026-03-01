#pragma once
// ipc/message_bus.h — Pure in-process pub/sub message dispatcher.
//
// All dispatch is lock-free. Subscriptions must be performed during
// the initialization phase (e.g. in service constructors) before any
// threads start publishing. The start of any thread that publishes
// serves as a memory barrier for the handler map.

#include <cstdint>
#include <cstring>

#include "msg_base.h"
#include "publisher.h"

namespace ipc {

// Raw dispatch implementation in message_bus.cpp
// Helper is already in publisher.h as a forward decl if needed,
// but we keep the implementation details here or in .cpp.

}  // namespace ipc
