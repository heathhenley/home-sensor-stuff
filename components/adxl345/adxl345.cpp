#include "adxl345.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace adxl345 {

static const char *const TAG = "adxl345";

// Register map
static const uint8_t REG_DEVID       = 0x00;
static const uint8_t REG_POWER_CTL   = 0x2D;
static const uint8_t REG_DATA_FORMAT = 0x31;
static const uint8_t REG_DATAX0      = 0x32;
static const uint8_t REG_INT_ENABLE  = 0x2E;
static const uint8_t REG_INT_SOURCE  = 0x30;
static const uint8_t REG_ACT_INACT_CTL = 0x27;
static const uint8_t REG_THRESH_ACT   = 0x24; // 62.5mg/LSB

// REG_DATA_FORMAT bits
static const uint8_t FULL_RES_SHIFT = 3;
static const uint8_t FULL_RES_WIDTH = 1;
static const uint8_t RANGE_SHIFT = 0;
static const uint8_t RANGE_WIDTH = 2;

// REG_POWER_CTL bits
static const uint8_t MEASURE_BIT_SHIFT = 3;
static const uint8_t MEASURE_BIT_WIDTH = 1;

// REG_INT_ENABLE + REG_INT_SOURCE bits
static const uint8_t ACTIVITY_BIT_SHIFT = 4;
static const uint8_t ACTIVITY_BIT_WIDTH = 1;

// REG_ACT_INACT_CTL bits
static const uint8_t ACT_ACDC_SHIFT = 7;
static const uint8_t ACT_ACDC_WIDTH = 1;
static const uint8_t ACT_X_ENABLE_SHIFT = 6;
static const uint8_t ACT_X_ENABLE_WIDTH = 1;
static const uint8_t ACT_Y_ENABLE_SHIFT = 5;
static const uint8_t ACT_Y_ENABLE_WIDTH = 1;
static const uint8_t ACT_Z_ENABLE_SHIFT = 4;
static const uint8_t ACT_Z_ENABLE_WIDTH = 1;


// Labels
static constexpr std::array<const char*, 4> RANGE_STRINGS = {
  "2G", "4G", "8G", "16G"
};
static constexpr std::array<const char*, 2> RES_STRINGS = {
   "10 bit", "Full"
};
static constexpr std::array<const char*, 3> AXIS_STRINGS = {
  "X", "Y", "Z"
};
static constexpr std::array<const char*, 2> COUPLING_STRINGS = {
  "DC", "AC"
};


// g/LSB for each range in 10 bit resolution mode
static constexpr std::array<float, 4> G_PER_LSB = {
  0.0039f, // 2G
  0.0078f, // 4G
  0.0156f, // 8G
  0.0312f, // 16G
};

// g/LSB for full resolution mode (same for all ranges)
static constexpr float G_PER_LSB_FULL_RES = 0.0039f;

// g/LSB for activity threshold
static constexpr float G_PER_LSB_ACTIVITY = 0.0625f;

uint8_t read_bits(uint8_t reg, const uint8_t shift, const uint8_t width) {
  uint8_t mask = (1 << width) - 1;
  return (reg >> shift) & mask;
}

uint8_t update_bits(
  uint8_t reg,
  const uint8_t shift,
  const uint8_t width,
  const uint8_t value
) {
  uint8_t mask = ((1 << width) - 1) << shift;
  return (reg & ~mask) | ((value << shift) & mask);
}

float g_per_lsb(const uint8_t full_res, const uint8_t range) {
  if (full_res || range == 0) {
    return G_PER_LSB_FULL_RES;
  }
  return G_PER_LSB[range];
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
  uint8_t data_format;
  if (this->read_register(REG_DATA_FORMAT, &data_format, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to read data format");
    this->mark_failed();
    return;
  }
  data_format = update_bits(
    data_format, RANGE_SHIFT, RANGE_WIDTH, this->range_);
  data_format = update_bits(
    data_format, FULL_RES_SHIFT, FULL_RES_WIDTH, this->full_res_);
  if (this->write_register(REG_DATA_FORMAT, &data_format, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to write data format");
    this->mark_failed();
    return;
  };

  // Set activity configuration
  if (this->activity_config_.activity_sensor != nullptr) {
    uint8_t act_inact_ctl;
    if (this->read_register(REG_ACT_INACT_CTL, &act_inact_ctl, 1) != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Failed to read activity configuration");
      this->mark_failed();
      return;
    };
    act_inact_ctl = update_bits(
      act_inact_ctl, ACT_ACDC_SHIFT, ACT_ACDC_WIDTH, this->activity_config_.coupling);
    act_inact_ctl = update_bits(
      act_inact_ctl, ACT_X_ENABLE_SHIFT, ACT_X_ENABLE_WIDTH, (this->activity_config_.axis_mask >> 0) & 1);
    act_inact_ctl = update_bits(
      act_inact_ctl, ACT_Y_ENABLE_SHIFT, ACT_Y_ENABLE_WIDTH, (this->activity_config_.axis_mask >> 1) & 1);
    act_inact_ctl = update_bits(
      act_inact_ctl, ACT_Z_ENABLE_SHIFT, ACT_Z_ENABLE_WIDTH, (this->activity_config_.axis_mask >> 2) & 1);
    if (this->write_register(REG_ACT_INACT_CTL, &act_inact_ctl, 1) != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Failed to write activity configuration");
      this->mark_failed();
      return;
    };

    // Set threshold
    uint8_t threshold_act_lsb = (uint8_t)(
      this->activity_config_.threshold_g / G_PER_LSB_ACTIVITY
    );
    // Make sure the threshold is at least 1 LSB, datasheet warns about int
    // active and threshold = 0
    if (threshold_act_lsb < 1) {
      threshold_act_lsb = 1;
    }
    ESP_LOGD(TAG, "Threshold: %.2f g -> %d LSB", this->activity_config_.threshold_g, threshold_act_lsb);
    if (this->write_register(REG_THRESH_ACT, &threshold_act_lsb, 1) != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Failed to write threshold");
      this->mark_failed();
      return;
    };
    // Enable activity interrupt
    uint8_t int_enable;
    if (this->read_register(REG_INT_ENABLE, &int_enable, 1) != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Failed to read interrupt enable");
      this->mark_failed();
      return;
    };
    int_enable = update_bits(
      int_enable, ACTIVITY_BIT_SHIFT, ACTIVITY_BIT_WIDTH, 1);
    if (this->write_register(REG_INT_ENABLE, &int_enable, 1) != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Failed to write interrupt enable");
      this->mark_failed();
      return;
    }
  }

  // Set power control to measure mode
  uint8_t power_ctl;
  if (this->read_register(REG_POWER_CTL, &power_ctl, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to read power control");
    this->mark_failed();
    return;
  };
  power_ctl = update_bits(
    power_ctl, MEASURE_BIT_SHIFT, MEASURE_BIT_WIDTH, 1);
  if (this->write_register(REG_POWER_CTL, &power_ctl, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to write power control");
    this->mark_failed();
    return;
  };

  ESP_LOGD(TAG, "ADXL345 setup complete");
}

void ADXL345Component::dump_config() {
  ESP_LOGCONFIG(TAG, "ADXL345:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  ESP_LOGCONFIG(TAG, "  Range: %s", RANGE_STRINGS[this->range_]);
  ESP_LOGCONFIG(TAG, "  Resolution: %s", RES_STRINGS[this->full_res_]);

  if (this->accel_x_ != nullptr)
    LOG_SENSOR("  ", "Acceleration X", this->accel_x_);
  if (this->accel_y_ != nullptr)
    LOG_SENSOR("  ", "Acceleration Y", this->accel_y_);
  if (this->accel_z_ != nullptr)
    LOG_SENSOR("  ", "Acceleration Z", this->accel_z_);

  if (this->activity_config_.activity_sensor != nullptr) {
    ESP_LOGCONFIG(TAG, "  Activity: Enabled");
    ESP_LOGCONFIG(
      TAG, "    Threshold: %.2f g", this->activity_config_.threshold_g);
    ESP_LOGCONFIG(
      TAG,
      "    Coupling: %s",
      COUPLING_STRINGS[this->activity_config_.coupling]
    );

    ESP_LOGCONFIG(TAG, "    Using axes: ");
    for (uint8_t i = 0; i < 3; i++) {
      if (this->activity_config_.axis_mask & (1 << i)) {
        ESP_LOGCONFIG(TAG, "      %s", AXIS_STRINGS[i]);
      }
    }
    LOG_BINARY_SENSOR("  ", "Activity", this->activity_config_.activity_sensor);
  }
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

  float scale = g_per_lsb(this->full_res_, this->range_);
  float x = raw_x * scale;
  float y = raw_y * scale;
  float z = raw_z * scale;

  if (this->accel_x_ != nullptr) this->accel_x_->publish_state(x);
  if (this->accel_y_ != nullptr) this->accel_y_->publish_state(y);
  if (this->accel_z_ != nullptr) this->accel_z_->publish_state(z);

  if (this->activity_config_.activity_sensor != nullptr) {
    uint8_t int_source;
    if (this->read_register(REG_INT_SOURCE, &int_source, 1) != i2c::ERROR_OK) {
      ESP_LOGW(TAG, "Failed to read interrupt source");
      return;
    }
    this->activity_config_.activity_sensor->publish_state(
      (int_source & (1 << ACTIVITY_BIT_SHIFT)) != 0
    );

  }

}

}  // namespace adxl345
}  // namespace esphome
