// ipc/message_bus.cpp

#include "message_bus.h"

#include <sys/select.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace ipc {

MessageBus::MessageBus(std::string socket_path)
    : server_(socket_path, /*create=*/true), client_(socket_path, /*create=*/false) {
  if (::pipe(wake_) < 0) throw std::runtime_error(std::string("pipe(): ") + std::strerror(errno));
  listener_thread_ = std::thread(&MessageBus::listener_loop, this);
}

MessageBus::~MessageBus() {
  char b = 0;
  (void)::write(wake_[1], &b, 1);
  if (listener_thread_.joinable()) listener_thread_.join();
  ::close(wake_[0]);
  ::close(wake_[1]);
}

void MessageBus::subscribe(MsgId id, Handler h) {
  std::lock_guard lk(mu_);
  per_id_subs_[static_cast<uint16_t>(id)].push_back(std::move(h));
}

void MessageBus::subscribe_all(Handler h) {
  std::lock_guard lk(mu_);
  wildcard_subs_.push_back(std::move(h));
}

void MessageBus::listener_loop() {
  const int srv = server_.fd();
  const int pip = wake_[0];
  const int max = std::max(srv, pip) + 1;

  while (true) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(srv, &fds);
    FD_SET(pip, &fds);

    if (::select(max, &fds, nullptr, nullptr, nullptr) < 0) {
      if (errno == EINTR) continue;
      break;
    }
    if (FD_ISSET(pip, &fds)) break;
    if (FD_ISSET(srv, &fds)) {
      auto msg = server_.recv();
      if (msg) dispatch(std::move(*msg));
    }
  }
}

void MessageBus::dispatch(RawMessage msg) {
  // Snapshot handlers under lock, call them outside.
  std::vector<Handler> handlers;
  {
    std::lock_guard lk(mu_);
    auto it = per_id_subs_.find(static_cast<uint16_t>(msg.msgId));
    if (it != per_id_subs_.end()) handlers = it->second;
    for (const auto& h : wildcard_subs_) handlers.push_back(h);
  }
  for (auto& h : handlers) h(msg);
}

}  // namespace ipc
