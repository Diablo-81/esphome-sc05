import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, sensor, text_sensor, uart
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PARTS_PER_MILLION,
)

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "text_sensor"]

CONF_NH3 = "nh3"
CONF_ONLINE = "online"
CONF_STATUS = "status"
CONF_CRC_ERRORS = "crc_errors"
CONF_LOST_FRAMES = "lost_frames"
CONF_TIMEOUT_COUNTER = "timeout_counter"
CONF_COMMUNICATION_TIMEOUT = "communication_timeout"

sc05_ns = cg.esphome_ns.namespace("sc05")
SC05Component = sc05_ns.class_("SC05Component", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SC05Component),
            cv.Optional(CONF_NH3): sensor.sensor_schema(
                unit_of_measurement=UNIT_PARTS_PER_MILLION,
                accuracy_decimals=2,
                icon="mdi:molecule",
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ONLINE): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_CONNECTIVITY,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_STATUS): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_CRC_ERRORS): sensor.sensor_schema(
                accuracy_decimals=0,
                icon="mdi:counter",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_LOST_FRAMES): sensor.sensor_schema(
                accuracy_decimals=0,
                icon="mdi:counter",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TIMEOUT_COUNTER): sensor.sensor_schema(
                accuracy_decimals=0,
                icon="mdi:counter",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_COMMUNICATION_TIMEOUT, default="3s"): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if nh3_config := config.get(CONF_NH3):
        sens = await sensor.new_sensor(nh3_config)
        cg.add(var.set_nh3_sensor(sens))

    if online_config := config.get(CONF_ONLINE):
        sens = await binary_sensor.new_binary_sensor(online_config)
        cg.add(var.set_online_binary_sensor(sens))

    if status_config := config.get(CONF_STATUS):
        sens = await text_sensor.new_text_sensor(status_config)
        cg.add(var.set_status_text_sensor(sens))

    if crc_errors_config := config.get(CONF_CRC_ERRORS):
        sens = await sensor.new_sensor(crc_errors_config)
        cg.add(var.set_crc_errors_sensor(sens))

    if lost_frames_config := config.get(CONF_LOST_FRAMES):
        sens = await sensor.new_sensor(lost_frames_config)
        cg.add(var.set_lost_frames_sensor(sens))

    if timeout_counter_config := config.get(CONF_TIMEOUT_COUNTER):
        sens = await sensor.new_sensor(timeout_counter_config)
        cg.add(var.set_timeout_counter_sensor(sens))

    cg.add(var.set_communication_timeout(config[CONF_COMMUNICATION_TIMEOUT]))
