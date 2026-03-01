#include "services/sensor_service.h"

#include <chrono>
#include <cmath>
#include <thread>

namespace sil {

SensorService::SensorService(ipc::MessageBus& bus) : bus_(bus), logger_("sensor") {

}

void SensorService::on_message(const StatePayload& state_msg) {
  std::lock_guard lk{mu_};
  state_ = state_msg.state;
}

void SensorService::on_message(const SensorRequestPayload& req) {
  SystemState current_state;
  {
    std::lock_guard lk{mu_};
    current_state = state_;
  }

  // Generate a mock request ID based on location coordinates for testing simplicity
  // Real world this could be passed in the req, but user asked for "new request, ack message"
  // Let's just use 0 as a simple correlator if not provided in request. We didn't add it to req.
  uint32_t req_id = 0;

  // Only activate in Ready state
  if (current_state != SystemState::Ready) {
    logger_.warn("Ignoring SensorRequest because system is not Ready.");
    ::SensorAckPayload ack{};
    ack.request_id = req_id;
    ack.success = false;
    ack.reason = 1;  // Not Ready
    bus_.publish<MsgId::SensorAck>(ack);
    return;
  }

  // Send Ack
  ::SensorAckPayload ack{};
  ack.request_id = req_id;
  ack.success = true;
  ack.reason = 0;
  bus_.publish<MsgId::SensorAck>(ack);

  // 10 microsecond lockdown delay
  std::this_thread::sleep_for(std::chrono::microseconds(10));

  std::lock_guard lk{mu_};

  // Deterministic noise generation based on private seed
  // Coherent math function to avoid sudden jumps
  float x = req.target_location.x;
  float y = req.target_location.y;

  // Use sin/cos for smooth terrain
  float noise1 = std::sin(x * 0.5f + seed_) * std::cos(y * 0.5f - seed_);
  float noise2 = std::sin(x * 0.1f) + std::cos(y * 0.1f);

  float combined_noise = (noise1 + noise2) * 0.5f;  // Approx range [-1, 1]

  ::EnvironmentPayload env{};
  env.region_id = ++region_counter_;

  // Create a bounding box around the request
  env.bounds.min_pt = {x - 5.0f, y - 5.0f};
  env.bounds.max_pt = {x + 5.0f, y + 5.0f};

  // Base param + noise variation
  env.ambient_temp_c = 20.0f + (combined_noise * 15.0f);  // 5C to 35C
  env.incline_percent = combined_noise * 20.0f;           // -20% to 20%

  // Friction: valleys (negative noise) = high friction (mud), peaks = low (ice)
  env.surface_friction = 1.0f - (combined_noise * 0.5f);  // 0.5 to 1.5

  // Max speed lower limit on high incline/friction
  env.max_speed_rpm = 3000.0f - (std::abs(env.incline_percent) * 50.0f);

  logger_.info("Generated terrain at (%.1f, %.1f): Temp=%.1fC, Incline=%.1f%%, Friction=%.2f", x, y,
               env.ambient_temp_c, env.incline_percent, env.surface_friction);

  bus_.publish<MsgId::EnvironmentData>(env);
}

}  // namespace sil
