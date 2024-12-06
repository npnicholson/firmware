import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, i2c
from esphome.const import CONF_OUTPUT_ID, CONF_OUTPUT

DEPENDENCIES = ["i2c"]

i2c_light_nc = cg.esphome_ns.namespace("i2c_light")
I2CLight = i2c_light_nc.class_(
    "I2CLight", light.LightOutput, i2c.I2CDevice
)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(I2CLight),
        # cv.Required(CONF_OUTPUT): cv.use_id(output.FloatOutput),
    }
).extend(i2c.i2c_device_schema(None))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await i2c.register_i2c_device(var, config)

    # out = await cg.get_variable(config[CONF_OUTPUT])
    # cg.add(var.set_output(out))
