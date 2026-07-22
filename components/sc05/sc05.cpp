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

  this->publish_frame_age_();
}

void SC05Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SC05 UART Gas Sensor:");
  LOG_SENSOR("  ", "NH3", this->nh3_sensor_);
  LOG_SENSOR("  ", "Full Scale", this->full_scale_sensor_);
  LOG_BINARY_SENSOR("  ", "Online", this->online_binary_sensor_);
  LOG_TEXT_SENSOR("  ", "Status", this->status_text_sensor_);
  ESP_LOGCONFIG(TAG, "  Communication timeout: %ums", this->communication_timeout_ms_);
}

void SC05Component::parse_byte_(uint8_t byte) {
  if (this->frame_pos_ >= SC05_FRAME_LENGTH) {
    this->mark_lost_frame_("parser buffer overflow guard");
    this->frame_pos_ = 0;
  }

  if (this->frame_pos_ == 0) {
    if (byte != SC05_START_BYTE)
      return;
    this->frame_buffer_[this->frame_pos_++] = byte;
    return;
  }

  if (byte == SC05_START_BYTE) {
    this->mark_lost_frame_("resynchronized on start byte");
    this->frame_pos_ = 0;
    this->frame_buffer_[this->frame_pos_++] = byte;
    return;
  }

  if (this->frame_pos_ >= SC05_FRAME_LENGTH) {
    this->mark_lost_frame_("parser buffer overflow before write");
    this->frame_pos_ = 0;
    return;
  }

  this->frame_buffer_[this->frame_pos_++] = byte;

  if (this->frame_pos_ == SC05_FRAME_LENGTH) {
    this->handle_frame_(this->frame_buffer_);
    this->frame_pos_ = 0;
  }
}

void SC05Component::handle_frame_(const uint8_t *frame) {
  this->dump_frame_(frame);

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

  if (!this->dispatch_gas_frame_(parsed)) {
    this->publish_diagnostics_();
    return;
  }

  this->last_valid_frame_ms_ = millis();
  this->timed_out_ = false;
  this->publish_online_(true, "Online");
  this->publish_diagnostics_();
}

bool SC05Component::validate_checksum_(const uint8_t *frame) const { return this->calculate_checksum_(frame) == frame[8]; }

uint8_t SC05Component::calculate_checksum_(const uint8_t *frame) const {
  // TODO: Verify checksum against captured UART frames from real SC05 sensor.
  // The checksum mode is intentionally explicit so new datasheet-confirmed
  // algorithms can be added without touching the parser state machine.
  switch (this->checksum_mode_) {
    case ChecksumMode::SUM_LOW_BYTE: {
      uint16_t checksum = 0;
      for (uint8_t i = 1; i <= 7; i++)
        checksum += frame[i];
      return checksum & 0xFF;
    }
  }
  return 0;
}

bool SC05Component::validate_nh3_fields_(const SC05Frame &frame) {
  if (frame.unit != SC05_UNIT_PPM) {
    this->invalid_frame_counter_++;
    this->publish_online_(false, "Invalid frame");
    ESP_LOGW(TAG, "Invalid SC05 unit for NH3: expected 0x%02X, got 0x%02X", SC05_UNIT_PPM, frame.unit);
    return false;
  }

  if (frame.decimals != SC05_DECIMALS_NH3) {
    this->invalid_frame_counter_++;
    this->publish_online_(false, "Invalid frame");
    ESP_LOGW(TAG, "Invalid SC05 decimal byte for NH3: expected 0x%02X, got 0x%02X", SC05_DECIMALS_NH3,
             frame.decimals);
    return false;
  }

  return true;
}

bool SC05Component::dispatch_gas_frame_(const SC05Frame &frame) {
  switch (frame.gas_id) {
    case SC05_GAS_ID_NH3: {
      if (!this->validate_nh3_fields_(frame))
        return false;

      const float ppm = frame.raw_concentration / 100.0f;
      if (this->nh3_sensor_ != nullptr)
        this->nh3_sensor_->publish_state(ppm);
      if (this->full_scale_sensor_ != nullptr)
        this->full_scale_sensor_->publish_state(frame.full_scale);
      ESP_LOGD(TAG, "NH3 frame: %.2f ppm, unit=0x%02X, decimals=%u, full_scale=%u", ppm, frame.unit, frame.decimals,
               frame.full_scale);
      return true;
    }
    default:
      this->publish_online_(false, "Unsupported gas");
      ESP_LOGW(TAG, "Unsupported SC05 gas id: 0x%02X", frame.gas_id);
      return false;
  }
}

void SC05Component::publish_frame_age_() {
  if (this->last_frame_age_sensor_ != nullptr)
    this->last_frame_age_sensor_->publish_state(millis() - this->last_valid_frame_ms_);
}

void SC05Component::dump_frame_(const uint8_t *frame) const {
  ESP_LOGV(TAG, "SC05 frame: %02X %02X %02X %02X %02X %02X %02X %02X %02X", frame[0], frame[1], frame[2], frame[3],
           frame[4], frame[5], frame[6], frame[7], frame[8]);
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
  if (this->invalid_frame_counter_sensor_ != nullptr)
    this->invalid_frame_counter_sensor_->publish_state(this->invalid_frame_counter_);
  if (this->timeout_counter_sensor_ != nullptr)
    this->timeout_counter_sensor_->publish_state(this->timeout_counter_);
  this->publish_frame_age_();
}

void SC05Component::mark_lost_frame_(const char *reason) {
  this->lost_frames_++;
  ESP_LOGD(TAG, "Lost SC05 frame: %s", reason);
}

}  // namespace sc05
}  // namespace esphome
