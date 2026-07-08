# ADXL345 Component for ESPHome

This is a custom component for ESPHome that interfaces with the ADXL345 3-axis accelerometer.

This is fork of original [here](https://github.com/jdillenburg/esphome/tree/main/components/adxl345). Fixed a bug with hardcoded mg/LSB constant
in fixed resolution mode and exposed it as a configuration option.

Datasheet: [ADXL345](https://www.analog.com/media/en/technical-documentation/data-sheets/adxl345.pdf)

## Features

- Supports all standard I2C configuration options including multiple I2C buses
- Configurable measurement range (2G, 4G, 8G, 16G)
- Configurable fixed (10 bit) or full resolution mode (up to 13 bit)
- Provides raw acceleration values for X, Y, and Z axes
- Configurable activity threshold and coupling (AC or DC) using on chip
  activity detection logic
- Full Home Assistant integration with proper device classes and units

## Installation

1. Create a `components` directory in your ESPHome configuration directory if it doesn't exist
2. Create an `adxl345` directory inside the `components` directory
3. Copy the following files into the `adxl345` directory:
   - `__init__.py`
   - `adxl345.h`
   - `adxl345.cpp`
   - `sensor.py`

## Dependencies

This component requires the following libraries:
- Wire
- SPI
- Adafruit BusIO
- Adafruit Unified Sensor
- Adafruit ADXL345

Add these to your ESPHome configuration:

```yaml
libraries:
  - "Wire"
  - "SPI"
  - "Adafruit BusIO"
  - "Adafruit Unified Sensor"
  - "Adafruit ADXL345"
```

## Usage

### Basic Configuration

```yaml
# Define component and sensors for accel_x, accel_y and accel_z
adxl345:
  - id: my_adxl345
    address: 0x53
    update_interval: 100ms
    accel_x:
      name: "Acceleration X"
    accel_y:
      name: "Acceleration Y"
    accel_z:
      name: "Acceleration Z"
```

### Advanced Configuration

```yaml
# Define I2C bus
i2c:
  sda: GPIO21
  scl: GPIO22
  frequency: 400kHz

# Define component with all options
adxl345:
  - id: my_adxl345
    address: 0x53
    update_interval: 100ms
    range: 4G  # Options: 2G, 4G, 8G, 16G
    accel_x:
      name: "Acceleration X"
      filters:
        - median:
            window_size: 5
            send_every: 5
            send_first_at: 1
    accel_y:
      name: "Acceleration Y"
    accel_z:
      name: "Acceleration Z"
```

## Available Sensors

| Sensor | Description | Unit |
|--------|-------------|------|
| `accel_x` | Raw X-axis acceleration | g |
| `accel_y` | Raw Y-axis acceleration | g |
| `accel_z` | Raw Z-axis acceleration | g |
| `activity` | Activity detection state | bool |

## Example Use Cases

- Bed/chair incline monitoring
- Movement detection
- Vibration monitoring
- Orientation sensing