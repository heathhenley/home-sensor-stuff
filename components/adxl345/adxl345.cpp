#include "adxl345.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace adxl345 {

static const char *const TAG = "adxl345";

static const float G_MS2 = 9.80665f;

// Register map
static const uint8_t REG_DEVID       = 0x00;
static const uint8_t REG_POWER_CTL   = 0x2D;
static const uint8_t REG_DATA_FORMAT = 0x31;
static const uint8_t REG_DATAX0      = 0x32;

float ms2_per_lsb(const uint8_t full_res, const uint8_t range) {
  if (full_res || range == 0) {
    return 0.0039f * G_MS2;
  }
  // 10 bit resolution - res changes
  switch (range) {
    case 1: return 0.0078f * G_MS2;
    case 2: return 0.0156f * G_MS2;
    case 3: return 0.0312f * G_MS2;
    default: return 0.0039f * G_MS2;
  }
}

uint8_t data_format_bits(const uint8_t range) {
  switch (range) {
    case 0: return 0x00; // ±2g
    case 1: return 0x01; // ±4g
    case 2: return 0x02; // ±8g
    case 3: return 0x03; // ±16g
    default: return 0x00;
  }
}


void ADXL345Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ADXL345...");

  // Check device ID
  uint8_t devid;
  if (this->read_register(REG_DEVID, &devid, 1) != i2c::ERROR_OK || devid != 0xE5) {
    ESP_LOGE(TAG, "ADXL345 not found at 0x%02X (id=0x%02X)", this->address_, devid);
    this->mark_failed();
    return;
  }
  ESP_LOGI(TAG, "Found ADXL345 at 0x%02X", this->address_);

  // Set range and full resolution bits
  uint8_t data_format = data_format_bits(this->range_) | this->full_res_;
  this->write_register(REG_DATA_FORMAT, &data_format, 1);

  // Enable measurement mode
  uint8_t power = 0x08;
  this->write_register(REG_POWER_CTL, &power, 1);

  ESP_LOGD(TAG, "ADXL345 setup complete");
}

void ADXL345Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ADXL345:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

  const char* range_str;
  switch (this->range_) {
    case 0: range_str = "2G"; break;
    case 1: range_str = "4G"; break;
    case 2: range_str = "8G"; break;
    case 3: range_str = "16G"; break;
    default: range_str = "Unknown"; break;
  }
  ESP_LOGCONFIG(TAG, "  Range: %s", range_str);
  ESP_LOGCONFIG(
    TAG, "  Resolution: %s", this->full_res_ ? "Full" : "Fixed (10 bit)");

  if (this->off_vertical_ != nullptr) LOG_SENSOR("  ", "Off Vertical", this->off_vertical_);
  if (this->jitter_ != nullptr)       LOG_SENSOR("  ", "Jitter", this->jitter_);
  if (this->accel_x_ != nullptr)      LOG_SENSOR("  ", "Acceleration X", this->accel_x_);
  if (this->accel_y_ != nullptr)      LOG_SENSOR("  ", "Acceleration Y", this->accel_y_);
  if (this->accel_z_ != nullptr)      LOG_SENSOR("  ", "Acceleration Z", this->accel_z_);
}

void ADXL345Component::update() {
  uint8_t buffer[6];
  if (this->read_register(REG_DATAX0, buffer, 6) != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "Failed to read data");
    return;
  }

  int16_t raw_x = (int16_t)(buffer[1] << 8 | buffer[0]);
  int16_t raw_y = (int16_t)(buffer[3] << 8 | buffer[2]);
  int16_t raw_z = (int16_t)(buffer[5] << 8 | buffer[4]);

  float scale = ms2_per_lsb(this->full_res_, this->range_);
  float x = raw_x * scale;
  float y = raw_y * scale;
  float z = raw_z * scale;

  if (this->accel_x_ != nullptr) this->accel_x_->publish_state(x);
  if (this->accel_y_ != nullptr) this->accel_y_->publish_state(y);
  if (this->accel_z_ != nullptr) this->accel_z_->publish_state(z);

  if (this->off_vertical_ != nullptr) {
    double pitch = atan(y / sqrt(pow(x, 2) + pow(z, 2))) * 180.0 / M_PI;
    double roll  = atan(-x / sqrt(pow(y, 2) + pow(z, 2))) * 180.0 / M_PI;
    this->off_vertical_->publish_state(std::max(std::abs(pitch), std::abs(roll)));
  }

  if (this->jitter_ != nullptr) {
    float jitter = fabs(x) + fabs(y) + fabs(z);
    this->jitter_->publish_state(jitter);
  }
}

}  // namespace adxl345
}  // namespace esphome
