#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace adxl345 {

struct ActivityConfig {
  float threshold_g;
  uint8_t coupling;
  uint8_t axis_mask;
  binary_sensor::BinarySensor *activity_sensor;
};

class ADXL345Component : public PollingComponent, public i2c::I2CDevice {
 public:
  ADXL345Component() : PollingComponent(1000) {}

  void setup() override;
  void update() override;
  void dump_config() override;

  void set_range(uint8_t range) { range_ = range; }
  void set_full_res(bool full_res) { full_res_ = full_res; }

  void set_accel_x_sensor(sensor::Sensor *accel_x) { accel_x_ = accel_x; }
  void set_accel_y_sensor(sensor::Sensor *accel_y) { accel_y_ = accel_y; }
  void set_accel_z_sensor(sensor::Sensor *accel_z) { accel_z_ = accel_z; }

  void set_threshold_g(float threshold_g) { activity_config_.threshold_g = threshold_g; }
  void set_coupling(uint8_t coupling) { activity_config_.coupling = coupling; }
  void set_axis(uint8_t axis) { activity_config_.axis_mask |= 1 << axis; }
  void set_activity_sensor(
    binary_sensor::BinarySensor *activity) { activity_config_.activity_sensor = activity; }

 protected:
  sensor::Sensor *accel_x_{nullptr};
  sensor::Sensor *accel_y_{nullptr};
  sensor::Sensor *accel_z_{nullptr};
  uint8_t range_{0};
  uint8_t full_res_{0};
  ActivityConfig activity_config_{
    .threshold_g = 0.0, .coupling = 0, .axis_mask = 0, .activity_sensor = nullptr
  };
};

}  // namespace adxl345
}  // namespace esphome
