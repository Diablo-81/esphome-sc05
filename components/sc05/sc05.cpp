#include "sc05.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sc05 {

static const char *const TAG = "sc05";

void SC05Component::setup() {
  this->frame_pos_ = 0;
  this->last_byte_ms_ = millis();
  this->last_valid_frame_ms_ = millis();
  this->publish_online_(false, "Waiting for SC05 frame");
  this->publish_diagnostics_();
}

void SC05Component::loop() {
  const uint32_t now = millis();

  while (this->available() > 0) {
    uint8_t byte;
    if (!this->read_byte(&byte))
      break;
    this->last_byte_ms_ = now;
    this->parse_byte_(byte);
  }

  if (this->frame_pos_ != 0 && now - this->last_byte_ms_ > 1000) {
    this->mark_lost_frame_("inter-byte timeout");
    this->frame_pos_ = 0;
  }

  if (!this->timed_out_ && now - this->last_valid_frame_ms_ > this->communication_timeout_ms_) {
    this->timeout_counter_++;
    this->timed_out_ = true;
    this->publish_online_(false, "Communication timeout");
    this->publish_diagnostics_();
    ESP_LOGD(TAG, "SC05 communication timeout");
  }
}

void SC05Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SC05 UART Gas Sensor:");
  LOG_SENSOR("  ", "NH3", this->nh3_sensor_);
  LOG_BINARY_SENSOR("  ", "Online", this->online_binary_sensor_);
  LOG_TEXT_SENSOR("  ", "Status", this->status_text_sensor_);
  ESP_LOGCONFIG(TAG, "  Communication timeout: %ums", this->communication_timeout_ms_);
}

void SC05Component::parse_byte_(uint8_t byte) {
  if (this->frame_pos_ == 0) {
    if (byte != SC05_START_BYTE)
      return;
    this->frame_buffer_[this->frame_pos_++] = byte;
    return;
  }

  if (byte == SC05_START_BYTE && this->frame_pos_ != 0) {
    this->mark_lost_frame_("resynchronized on start byte");
    this->frame_pos_ = 0;
    this->frame_buffer_[this->frame_pos_++] = byte;
    return;
  }

  this->frame_buffer_[this->frame_pos_++] = byte;

  if (this->frame_pos_ == SC05_FRAME_LENGTH) {
    this->handle_frame_(this->frame_buffer_);
    this->frame_pos_ = 0;
  }
}

void SC05Component::handle_frame_(const uint8_t *frame) {
  if (!this->validate_checksum_(frame)) {
    this->crc_errors_++;
    this->publish_diagnostics_();
    ESP_LOGD(TAG, "Invalid SC05 checksum: expected 0x%02X, got 0x%02X", this->calculate_checksum_(frame), frame[8]);
    return;
  }

  SC05Frame parsed{};
  parsed.gas_id = frame[1];
  parsed.unit = frame[2];
  parsed.decimals = frame[3];
  parsed.raw_concentration = (uint16_t(frame[4]) << 8) | frame[5];
  parsed.full_scale = (uint16_t(frame[6]) << 8) | frame[7];

  this->last_valid_frame_ms_ = millis();
  this->timed_out_ = false;
  this->publish_online_(true, "Online");

  if (parsed.gas_id == SC05_GAS_ID_NH3) {
    const float ppm = parsed.raw_concentration / 100.0f;
    if (this->nh3_sensor_ != nullptr)
      this->nh3_sensor_->publish_state(ppm);
    ESP_LOGD(TAG, "NH3 frame: %.2f ppm, unit=0x%02X, decimals=%u, full_scale=%u", ppm, parsed.unit, parsed.decimals,
             parsed.full_scale);
  } else {
    this->mark_lost_frame_("unsupported gas id");
    ESP_LOGD(TAG, "Unsupported SC05 gas id: 0x%02X", parsed.gas_id);
  }

  this->publish_diagnostics_();
}

bool SC05Component::validate_checksum_(const uint8_t *frame) const { return this->calculate_checksum_(frame) == frame[8]; }

uint8_t SC05Component::calculate_checksum_(const uint8_t *frame) const {
  // Datasheets for this sensor family usually describe the checksum as the low
  // byte of the sum of bytes 1..7. Keeping the algorithm isolated here makes it
  // straightforward to adjust if a specific SC05 variant documents a different
  // checksum formula.
  uint16_t checksum = 0;
  for (uint8_t i = 1; i <= 7; i++)
    checksum += frame[i];
  return checksum & 0xFF;
}

void SC05Component::publish_online_(bool online, const char *status) {
  if (this->online_ != online || !this->online_published_) {
    this->online_ = online;
    this->online_published_ = true;
    if (this->online_binary_sensor_ != nullptr)
      this->online_binary_sensor_->publish_state(online);
  }
  if (this->status_text_sensor_ != nullptr)
    this->status_text_sensor_->publish_state(status);
}

void SC05Component::publish_diagnostics_() {
  if (this->crc_errors_sensor_ != nullptr)
    this->crc_errors_sensor_->publish_state(this->crc_errors_);
  if (this->lost_frames_sensor_ != nullptr)
    this->lost_frames_sensor_->publish_state(this->lost_frames_);
  if (this->timeout_counter_sensor_ != nullptr)
    this->timeout_counter_sensor_->publish_state(this->timeout_counter_);
}

void SC05Component::mark_lost_frame_(const char *reason) {
  this->lost_frames_++;
  ESP_LOGD(TAG, "Lost SC05 frame: %s", reason);
}

}  // namespace sc05
}  // namespace esphome
