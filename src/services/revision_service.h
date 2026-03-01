#pragma once
// services/revision_service.h

#include "publisher.h"
#include "core_msgs.h"

namespace sil {

class DOC_DESC("Provides runtime verification of the compiled IPC protocol hash.") RevisionService {
 public:
  using Subscribes = ipc::MsgList<MsgId::RevisionRequest>;
  using Publishes = ipc::MsgList<MsgId::RevisionResponse>;

  explicit RevisionService(ipc::MessageBus& bus);

  void start() {
  }  // No internal thread required

  void on_message(const RevisionRequestPayload& req);

 private:
  ipc::TypedPublisher<RevisionService> bus_;
};

}  // namespace sil
