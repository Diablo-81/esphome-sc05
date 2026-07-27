#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace sc05 {

static constexpr uint8_t SC05_FRAME_LENGTH = 9;
static constexpr uint8_t SC05_START_BYTE = 0xFF;
static constexpr uint8_t SC05_GAS_ID_NH3 = 0x17;
static constexpr uint8_t SC05_UNIT_PPM = 0x04;
static constexpr uint8_t SC05_DECIMALS_NH3 = 0x00;
static constexpr uint32_t SC05_DEFAULT_TIMEOUT_MS = 3000;

/// Checksum algorithms known by this driver.
enum class ChecksumMode : uint8_t {
  SUM_LOW_BYTE,
};

/// Parsed SC05 frame independent from ESPHome entity publishing.
struct SC05Frame {
  uint8_t gas_id;
  uint8_t unit;
  uint8_t decimals;
  uint16_t raw_concentration;
  uint16_t full_scale;
};

/// ESPHome UART component for the SC05 family of automatic-reporting gas sensors.
class SC05Component : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_nh3_sensor(sensor::Sensor *sensor) { nh3_sensor_ = sensor; }
  void set_online_binary_sensor(binary_sensor::BinarySensor *sensor) { online_binary_sensor_ = sensor; }
  void set_status_text_sensor(text_sensor::TextSensor *sensor) { status_text_sensor_ = sensor; }
  void set_full_scale_sensor(sensor::Sensor *sensor) { full_scale_sensor_ = sensor; }
  void set_crc_errors_sensor(sensor::Sensor *sensor) { crc_errors_sensor_ = sensor; }
  void set_lost_frames_sensor(sensor::Sensor *sensor) { lost_frames_sensor_ = sensor; }
  void set_invalid_frame_counter_sensor(sensor::Sensor *sensor) { invalid_frame_counter_sensor_ = sensor; }
  void set_timeout_counter_sensor(sensor::Sensor *sensor) { timeout_counter_sensor_ = sensor; }
  void set_last_frame_age_sensor(sensor::Sensor *sensor) { last_frame_age_sensor_ = sensor; }
  void set_communication_timeout(uint32_t timeout_ms) { communication_timeout_ms_ = timeout_ms; }

 protected:
  void parse_byte_(uint8_t byte);
  void handle_frame_(const uint8_t *frame);
  bool validate_checksum_(const uint8_t *frame) const;
  uint8_t calculate_checksum_(const uint8_t *frame) const;
  bool validate_nh3_fields_(const SC05Frame &frame);
  bool dispatch_gas_frame_(const SC05Frame &frame);
  void publish_frame_age_();
  void dump_frame_(const uint8_t *frame) const;
  void publish_online_(bool online, const char *status);
  void publish_diagnostics_();
  void mark_lost_frame_(const char *reason);

  uint8_t frame_buffer_[SC05_FRAME_LENGTH]{};
  uint8_t frame_pos_{0};
  uint32_t last_byte_ms_{0};
  uint32_t last_valid_frame_ms_{0};
  uint32_t communication_timeout_ms_{SC05_DEFAULT_TIMEOUT_MS};
  bool online_{false};
  bool online_published_{false};
  bool timed_out_{false};

  uint32_t crc_errors_{0};
  uint32_t lost_frames_{0};
  uint32_t invalid_frame_counter_{0};
  uint32_t timeout_counter_{0};

  sensor::Sensor *nh3_sensor_{nullptr};
  sensor::Sensor *full_scale_sensor_{nullptr};
  binary_sensor::BinarySensor *online_binary_sensor_{nullptr};
  text_sensor::TextSensor *status_text_sensor_{nullptr};
  sensor::Sensor *crc_errors_sensor_{nullptr};
  sensor::Sensor *lost_frames_sensor_{nullptr};
  sensor::Sensor *invalid_frame_counter_sensor_{nullptr};
  sensor::Sensor *timeout_counter_sensor_{nullptr};
  sensor::Sensor *last_frame_age_sensor_{nullptr};
  ChecksumMode checksum_mode_{ChecksumMode::SUM_LOW_BYTE};
};

}  // namespace sc05
}  // namespace esphome
