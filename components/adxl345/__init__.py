import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, STATE_CLASS_MEASUREMENT
from esphome.components import i2c, sensor, binary_sensor

DEPENDENCIES = ['i2c']
AUTO_LOAD = ['sensor', 'binary_sensor']
MULTI_CONF = True

adxl345_ns = cg.esphome_ns.namespace('adxl345')
ADXL345Component = adxl345_ns.class_('ADXL345Component', cg.PollingComponent, i2c.I2CDevice)

CONF_RANGE = "range"
RANGE_2G = 0
RANGE_4G = 1
RANGE_8G = 2
RANGE_16G = 3

CONF_FULL_RES = "full_res"

CONF_ACCEL_X = "accel_x"
CONF_ACCEL_Y = "accel_y"
CONF_ACCEL_Z = "accel_z"

CONF_ACTIVITY = "activity"
CONF_ACTIVITY_THRESHOLD_G = "threshold_g"
CONF_ACTIVITY_COUPLING = "coupling"
COUPLING_DC = 0
COUPLING_AC = 1
CONF_ACTIVITY_AXIS = "axis"
AXIS_X = 0
AXIS_Y = 1
AXIS_Z = 2

CONF_ACTIVITY_BINARY_SENSOR = "binary_sensor"

ACTIVITY_SCHEMA = cv.Schema({
    cv.Required(CONF_ACTIVITY_THRESHOLD_G): cv.float_range(
        min=0.0625, max=15.9375
    ),
    cv.Optional(CONF_ACTIVITY_COUPLING, default="AC"): cv.enum({
        "AC": COUPLING_AC,
        "DC": COUPLING_DC,
    }, upper=True),
    cv.Optional(CONF_ACTIVITY_AXIS, default=["x", "y", "z"]): cv.ensure_list(
        cv.enum({
            "x": AXIS_X,
            "y": AXIS_Y,
            "z": AXIS_Z,
        }, lower=True)
    ),
    cv.Required(CONF_ACTIVITY_BINARY_SENSOR): binary_sensor.binary_sensor_schema(),
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ADXL345Component),
    cv.Optional(CONF_RANGE, default="2G"): cv.enum({
        "2G": RANGE_2G,
        "4G": RANGE_4G,
        "8G": RANGE_8G,
        "16G": RANGE_16G,
    }, upper=True),
    cv.Optional(CONF_FULL_RES): cv.boolean,
    cv.Optional(CONF_ACCEL_X): sensor.sensor_schema(
        unit_of_measurement="g",
        icon="mdi:axis-x-arrow",
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_ACCEL_Y): sensor.sensor_schema(
        unit_of_measurement="g",
        icon="mdi:axis-y-arrow",
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_ACCEL_Z): sensor.sensor_schema(
        unit_of_measurement="g",
        icon="mdi:axis-z-arrow",
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    cv.Optional(CONF_ACTIVITY): ACTIVITY_SCHEMA,
}).extend(cv.polling_component_schema("1s")).extend(i2c.i2c_device_schema(0x53))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    
    cg.add(var.set_range(config[CONF_RANGE]))

    if full_res := config.get(CONF_FULL_RES):
        cg.add(var.set_full_res(full_res))
    
    if accel_x := config.get(CONF_ACCEL_X):
        sens = await sensor.new_sensor(accel_x)
        cg.add(var.set_accel_x_sensor(sens))
        
    if accel_y := config.get(CONF_ACCEL_Y):
        sens = await sensor.new_sensor(accel_y)
        cg.add(var.set_accel_y_sensor(sens))
        
    if accel_z := config.get(CONF_ACCEL_Z):
        sens = await sensor.new_sensor(accel_z)
        cg.add(var.set_accel_z_sensor(sens))

    if activity := config.get(CONF_ACTIVITY):
        cg.add(var.set_threshold_g(activity[CONF_ACTIVITY_THRESHOLD_G]))
        cg.add(var.set_coupling(activity[CONF_ACTIVITY_COUPLING]))
        for axis in activity[CONF_ACTIVITY_AXIS]:
            cg.add(var.set_axis(axis))
        sens = await binary_sensor.new_binary_sensor(activity[CONF_ACTIVITY_BINARY_SENSOR])
        cg.add(var.set_activity_sensor(sens))
