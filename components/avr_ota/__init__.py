import logging
from esphome import automation
from esphome.automation import Condition, maybe_simple_id
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import spi, output
from esphome.const import CONF_ID, CONF_PORT, CONF_TRIGGER_ID, CONF_RESTORE_MODE
CONF_AVR_ENABLE = "avr_enable_output"

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@npnicholson"]
AUTO_LOAD = ["md5", "socket"]
DEPENDENCIES = ["network", "spi"]

CONF_HUB_ID = "avr_hub"
CONF_ON_ENABLE = "on_enable"
CONF_ON_DISABLE = "on_disable"

CONF_ON_AVR_IDLE = "on_avr_idle"
CONF_ON_AVR_PENDING = "on_avr_pending"
CONF_ON_AVR_ACTIVE = "on_avr_active"
CONF_ON_AVR_FAILURE = "on_avr_failure"

avr_ota_ns = cg.esphome_ns.namespace("avr_ota")
AVROTAComponent = avr_ota_ns.class_("AVROTAComponent",  cg.Component, spi.SPIDevice)

AVRRestoreMode = avr_ota_ns.enum("AVRRestoreMode_t")
RESTORE_MODES = {
    "RESTORE_DEFAULT_OFF": AVRRestoreMode.AVR_RESTORE_DEFAULT_OFF,
    "RESTORE_DEFAULT_ON": AVRRestoreMode.AVR_RESTORE_DEFAULT_ON,
    "ALWAYS_OFF": AVRRestoreMode.AVR_ALWAYS_OFF,
    "ALWAYS_ON": AVRRestoreMode.AVR_ALWAYS_ON,
    "RESTORE_INVERTED_DEFAULT_OFF": AVRRestoreMode.AVR_RESTORE_INVERTED_DEFAULT_OFF,
    "RESTORE_INVERTED_DEFAULT_ON": AVRRestoreMode.AVR_RESTORE_INVERTED_DEFAULT_ON,
    "RESTORE_AND_OFF": AVRRestoreMode.AVR_RESTORE_AND_OFF,
    "RESTORE_AND_ON": AVRRestoreMode.AVR_RESTORE_AND_ON,
}

ToggleAction = avr_ota_ns.class_("ToggleAction", automation.Action)
DisableAction = avr_ota_ns.class_("DisableAction", automation.Action)
EnableAction = avr_ota_ns.class_("EnableAction", automation.Action)
ResetAction = avr_ota_ns.class_("ResetAction", automation.Action)

AVRCondition = avr_ota_ns.class_("AVRCondition", Condition)

EnableTrigger = avr_ota_ns.class_("EnableTrigger", automation.Trigger.template())
DisableTrigger = avr_ota_ns.class_("DisableTrigger", automation.Trigger.template())

AVRIdleTrigger           = avr_ota_ns.class_("AVRIdleTrigger", automation.Trigger.template())
AVRPendingTrigger        = avr_ota_ns.class_("AVRPendingTrigger", automation.Trigger.template())
AVRActiveTrigger         = avr_ota_ns.class_("AVRActiveTrigger", automation.Trigger.template())
AVRForcedShutdownTrigger = avr_ota_ns.class_("AVRForcedShutdownTrigger", automation.Trigger.template())

AVRStateIdle = avr_ota_ns.class_("AVRStateIdle", Condition)
AVRStatePending = avr_ota_ns.class_("AVRStatePending", Condition)
AVRStateActive = avr_ota_ns.class_("AVRStateActive", Condition)
AVRStateForcedShutdown = avr_ota_ns.class_("AVRStateForcedShutdown", Condition)

ClientConnectedTrigger = avr_ota_ns.class_("EnableTrigger", automation.Trigger.template())
ClientDisconnectedTrigger = avr_ota_ns.class_("DisableTrigger", automation.Trigger.template())





CONFIG_SCHEMA = output.BINARY_OUTPUT_SCHEMA.extend(cv.Schema({
        cv.GenerateID(): cv.declare_id(AVROTAComponent),
        cv.Required(CONF_AVR_ENABLE): cv.use_id(output.BinaryOutput),
        cv.Optional(CONF_PORT, 328): cv.port,
        cv.Optional(CONF_RESTORE_MODE, default="ALWAYS_OFF"): cv.enum(
            RESTORE_MODES, upper=True, space="_"
        ),
        cv.Optional(CONF_ON_ENABLE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(EnableTrigger),
            }
        ),
        cv.Optional(CONF_ON_DISABLE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(DisableTrigger),
            }
        ),
        cv.Optional(CONF_ON_AVR_IDLE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(AVRIdleTrigger),
            }
        ),
        cv.Optional(CONF_ON_AVR_PENDING): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(AVRPendingTrigger),
            }
        ),
        cv.Optional(CONF_ON_AVR_ACTIVE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(AVRActiveTrigger),
            }
        ),
        cv.Optional(CONF_ON_AVR_FAILURE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(AVRForcedShutdownTrigger),
            }
        ),
    })
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=False))
)

CHILD_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HUB_ID): cv.use_id(AVROTAComponent),
    }
)

async def to_code(config):
    # Get a reference to the config id for this component
    var = cg.new_Pvariable(config[CONF_ID])

    # Set the config port from the config
    cg.add(var.set_ws_port(config[CONF_PORT]))

    # Set AVR Restore mode
    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))

    # Set the avr enable output from the config
    avr_enable = await cg.get_variable(config[CONF_AVR_ENABLE])
    cg.add(var.set_avr_enable(avr_enable))

    # Set up triggers
    for conf in config.get(CONF_ON_ENABLE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_DISABLE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_AVR_IDLE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_AVR_PENDING, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_AVR_ACTIVE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_AVR_FAILURE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    await spi.register_spi_device(var, config)
    await cg.register_component(var, config)

    

    # out = await cg.get_variable(config[CONF_OUTPUT])

AVR_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(AVROTAComponent),
    }
)

# Set up actions
@automation.register_action("avr_ota.toggle", ToggleAction, AVR_ACTION_SCHEMA)
@automation.register_action("avr_ota.enable", EnableAction, AVR_ACTION_SCHEMA)
@automation.register_action("avr_ota.disable", DisableAction, AVR_ACTION_SCHEMA)
@automation.register_action("avr_ota.reset", ResetAction, AVR_ACTION_SCHEMA)
async def avr_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)

## Set Up Conditions
@automation.register_condition("avr_ota.is_enabled", AVRCondition, AVR_ACTION_SCHEMA)
async def avr_enabled_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, True)

@automation.register_condition("avr_ota.is_disabled", AVRCondition, AVR_ACTION_SCHEMA)
async def avr_disabled_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, False)

@automation.register_condition("avr_ota.is_idle", AVRStateIdle, AVR_ACTION_SCHEMA)
async def avr_is_idle_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, True)

@automation.register_condition("avr_ota.is_pending", AVRStatePending, AVR_ACTION_SCHEMA)
async def avr_is_pending_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, True)

@automation.register_condition("avr_ota.is_active", AVRStateActive, AVR_ACTION_SCHEMA)
async def avr_is_active_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, True)

@automation.register_condition("avr_ota.is_error", AVRStateForcedShutdown, AVR_ACTION_SCHEMA)
async def avr_is_error_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, True)

