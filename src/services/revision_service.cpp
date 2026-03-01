#include "services/revision_service.h"

#include <cstring>

#include "revision.h"

namespace sil {

RevisionService::RevisionService(ipc::MessageBus& bus) : bus_(bus) {

}

void RevisionService::on_message(const RevisionRequestPayload& /*req*/) {
  ::RevisionResponsePayload resp{};

  // Copy the macro definition from the generated header
  std::strncpy(resp.protocol_hash, PROTOCOL_HASH, sizeof(resp.protocol_hash) - 1);
  resp.protocol_hash[sizeof(resp.protocol_hash) - 1] = '\0';

  bus_.publish<MsgId::RevisionResponse>(resp);
}

}  // namespace sil
