import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, core
from esphome.const import CONF_ID

empty_sensor_ns = cg.esphome_ns.namespace('alarmsystem')
EmptySensor = empty_sensor_ns.class_('vistaECPHome', cg.PollingComponent)

CONF_ACCESSCODE="accesscode"
CONF_MAXZONES="maxzones"
CONF_MAXPARTITIONS="maxpartitions"
CONF_RFSERIAL="rfseriallookup"
CONF_DEFAULTPARTITION="defaultpartition"
CONF_DEBUGLEVEL="vistadebuglevel"
CONF_KEYPAD1="keypadaddr1"
CONF_KEYPAD2="keypadaddr2"
CONF_KEYPAD3="keypadaddr3"
CONF_RXPIN="rxpin"
CONF_TXPIN="txpin"
CONF_MONITORPIN="monitorpin"
CONF_EXPANDER1="expanderaddr1"
CONF_EXPANDER2="expanderaddr2"
CONF_RELAY1="relayaddr1"
CONF_RELAY2="relayaddr2"
CONF_RELAY3="relayaddr3"
CONF_RELAY4="relayaddr4"
CONF_TTL="ttl"
CONF_QUICKARM="quickarm"
CONF_LRR="lrrsupervisor"


systemstatus= '''[&](std::string statusCode,uint8_t partition) {
       std::string sensor = "(ss_"+std::to_string(partition)+")";
      alarmsystem::publishTextState(&sensor,partition,&statusCode); 
    }'''
line1 ='''[&](std::string msg,uint8_t partition) {
      std::string sensor = "(ln1_"+std::to_string(partition)+")";
      alarmsystem::publishTextState(&sensor,partition,&msg);
    }'''
line2='''[&](std::string msg,uint8_t partition) {
      std::string sensor = "(ln2_"+std::to_string(partition)+")";
      alarmsystem::publishTextState(&sensor,partition,&msg);
    }'''
beeps='''[&](std::string  beeps,uint8_t partition) {
      std::string sensor = "(bp_"+std::to_string(partition)+")";
      alarmsystem::publishTextState(&sensor,partition,&beeps); 
    }'''
zoneext='''[&](std::string msg) {
      std::string sensor = "(zs)";
      alarmsystem::publishTextState(&sensor,0,&msg);  
    }'''
lrr='''[&](std::string msg) {
      std::string sensor = "(lrr)";
      alarmsystem::publishTextState(&sensor,0,&msg);  
    }''' 
rf='''[&](std::string msg) {
      std::string sensor = "(rf)";
      alarmsystem::publishTextState(&sensor,0,&msg);  
    }'''
statuschange='''[&](alarmsystem::sysState led,bool open,uint8_t partition) {
     std::string sensor="NIL";   
      switch(led) {
                case alarmsystem::sfire: sensor="(fire_";break;
                case alarmsystem::salarm: sensor="(alm_";break;
                case alarmsystem::strouble: sensor="(trbl_";break;
                case alarmsystem::sarmedstay:sensor="(arms_";break;
                case alarmsystem::sarmedaway: sensor="(arma_";break;
                case alarmsystem::sinstant: sensor="(armi_";break; 
                case alarmsystem::sready: sensor="(rdy_";break; 
                case alarmsystem::sac: sensor="(ac)"; alarmsystem::publishBinaryState(&sensor,partition,open);return;       
                case alarmsystem::sbypass: sensor="(byp_";break;  
                case alarmsystem::schime: sensor="(chm_";break;
                case alarmsystem::sbat: sensor="(bat)";alarmsystem::publishBinaryState(&sensor,partition,open);return;  
                case alarmsystem::sarmednight: sensor="(armn_";break;  
                case alarmsystem::sarmed: sensor="(arm_";break;  
                case alarmsystem::soffline: break;       
                case alarmsystem::sunavailable: break;                        
         };
      sensor=sensor  + std::to_string(partition) + ")";
      alarmsystem::publishBinaryState(&sensor,partition,open);
    }'''
zonebinary='''[&](int zone, bool open) {
      std::string sensor = "(z" + std::to_string(zone) + ")";
      alarmsystem::publishBinaryState(&sensor,0,open);    
    }'''
zonestatus='''[&](int zone, std::string open) {
      std::string sensor = "(z" + std::to_string(zone) + ")";
      alarmsystem::publishTextState(&sensor,0,&open); 
    }''' 
relay='''[&](uint8_t addr,int channel,bool open) {
      std::string sensor = "(r"+std::to_string(addr) + std::to_string(channel) + ")";
      alarmsystem::publishBinaryState(&sensor,0,open);       
    }'''



CONFIG_SCHEMA = cv.Schema(
    {
    cv.GenerateID(): cv.declare_id(EmptySensor),
    cv.Optional(CONF_ACCESSCODE, default=""): cv.string  ,
    cv.Optional(CONF_MAXZONES, default=""): cv.int_, 
    cv.Optional(CONF_MAXPARTITIONS, default=""): cv.int_, 
    cv.Optional(CONF_RFSERIAL, default=""): cv.string_strict, 
    cv.Optional(CONF_DEFAULTPARTITION, default=""): cv.int_, 
    cv.Optional(CONF_DEBUGLEVEL, default=""): cv.int_, 
    cv.Optional(CONF_KEYPAD1, default=""): cv.int_, 
    cv.Optional(CONF_KEYPAD2, default=""): cv.int_, 
    cv.Optional(CONF_KEYPAD3, default=""): cv.int_, 
    cv.Optional(CONF_RXPIN, default=""): cv.int_, 
    cv.Optional(CONF_TXPIN, default=""): cv.int_, 
    cv.Optional(CONF_MONITORPIN, default=""): cv.int_, 
    cv.Optional(CONF_EXPANDER1, default=""): cv.int_, 
    cv.Optional(CONF_EXPANDER2, default=""): cv.int_, 
    cv.Optional(CONF_RELAY1, default=""): cv.int_, 
    cv.Optional(CONF_RELAY2, default=""): cv.int_, 
    cv.Optional(CONF_RELAY3, default=""): cv.int_, 
    cv.Optional(CONF_RELAY4, default=""): cv.int_, 
    cv.Optional(CONF_TTL, default=""): cv.int_, 
    cv.Optional(CONF_QUICKARM, default=""): cv.boolean, 
    cv.Optional(CONF_LRR, default=""): cv.boolean,     
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID],config[CONF_KEYPAD1],config[CONF_RXPIN],config[CONF_TXPIN],config[CONF_MONITORPIN],config[CONF_MAXZONES],config[CONF_MAXPARTITIONS])
    
    if (config[CONF_ACCESSCODE]):
        cg.add(var.set_accessCode(config[CONF_ACCESSCODE]));
    if (config[CONF_MAXZONES]):
        cg.add(var.set_maxZones(config[CONF_MAXZONES]));
    if (config[CONF_MAXPARTITIONS]):
        cg.add(var.set_maxPartitions(config[CONF_MAXPARTITIONS]));
    if (config[CONF_RFSERIAL]):
        cg.add(var.set_rfSerialLookup(config[CONF_RFSERIAL]));
    if (config[CONF_DEFAULTPARTITION]):
        cg.add(var.set_defaultPartition(config[CONF_DEFAULTPARTITION]));
    if (config[CONF_DEBUGLEVEL]):
        cg.add(var.set_debug(config[CONF_DEBUGLEVEL]));
    if (config[CONF_KEYPAD1]):
        cg.add(var.set_partitionKeypad(1,config[CONF_KEYPAD1]));
    if (config[CONF_KEYPAD2]):
        cg.add(var.set_partitionKeypad(2,config[CONF_KEYPAD2]));
    if (config[CONF_KEYPAD3]):
        cg.add(var.set_partitionKeypad(3,config[CONF_KEYPAD3]));
    if (config[CONF_EXPANDER1]):
        cg.add(var.set_expanderAddr(1,config[CONF_EXPANDER1]));
    if (config[CONF_EXPANDER2]):
        cg.add(var.set_expanderAddr(2,config[CONF_EXPANDER2]));
    if (config[CONF_RELAY1]):
        cg.add(var.set_expanderAddr(3,config[CONF_RELAY1]));
    if (config[CONF_RELAY2]):
        cg.add(var.set_expanderAddr(4,config[CONF_RELAY2]));
    if (config[CONF_RELAY3]):
        cg.add(var.set_expanderAddr(5,config[CONF_RELAY3]));
    if (config[CONF_RELAY4]):
        cg.add(var.set_expanderAddr(6,config[CONF_RELAY4]));
    if (config[CONF_TTL]):
        cg.add(var.set_ttl(config[CONF_TTL]));        
    if (config[CONF_QUICKARM]):
        cg.add(var.set_quickArm(config[CONF_QUICKARM]));        
    if (config[CONF_LRR]):
        cg.add(var.set_lrrSupervisor(config[CONF_LRR]));       
        
    cg.add(var.onSystemStatusChange(cg.RawExpression(systemstatus)))   
    cg.add(var.onLine1DisplayChange(cg.RawExpression(line1))) 
    cg.add(var.onLine2DisplayChange(cg.RawExpression(line2)))    
    cg.add(var.onBeepsChange(cg.RawExpression(beeps)))    
    cg.add(var.onZoneExtendedStatusChange(cg.RawExpression(zoneext)))   
    cg.add(var.onLrrMsgChange(cg.RawExpression(lrr))) 
    cg.add(var.onRfMsgChange(cg.RawExpression(rf)))    
    cg.add(var.onStatusChange(cg.RawExpression(statuschange)))    
    cg.add(var.onZoneStatusChangeBinarySensor(cg.RawExpression(zonebinary)))    
    cg.add(var.onZoneStatusChange(cg.RawExpression(zonestatus)))
    cg.add(var.onRelayStatusChange(cg.RawExpression(relay)))      
    await cg.register_component(var, config)
    