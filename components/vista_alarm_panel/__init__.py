import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

component_ns = cg.esphome_ns.namespace('alarm_panel')
Component = component_ns.class_('vistaECPHome', cg.PollingComponent)

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
CONF_FAULT="fault_text"
CONF_BYPASS="bypas_text"
CONF_ALARM="alarm_text"
CONF_FIRE="fire_text"
CONF_CHECK="check_text"
CONF_TRBL="trbl_text"
CONF_HITSTAR="hitstar_text"

systemstatus= '''[&](std::string statusCode,uint8_t partition) {
      alarm_panel::publishTextState("ss_",partition,&statusCode); 
    }'''
line1 ='''[&](std::string msg,uint8_t partition) {
      alarm_panel::publishTextState("ln1_",partition,&msg);
    }'''
line2='''[&](std::string msg,uint8_t partition) {
      alarm_panel::publishTextState("ln2_",partition,&msg);
    }'''
beeps='''[&](std::string  beeps,uint8_t partition) {
      alarm_panel::publishTextState("bp_",partition,&beeps); 
    }'''
zoneext='''[&](std::string msg) {
      alarm_panel::publishTextState("zs",0,&msg);  
    }'''
lrr='''[&](std::string msg) {
      alarm_panel::publishTextState("lrr",0,&msg);  
    }''' 
rf='''[&](std::string msg) {
      alarm_panel::publishTextState("rf",0,&msg);  
    }'''
statuschange='''[&](alarm_panel::sysState led,bool open,uint8_t partition) {
     std::string sensor="NIL";   
      switch(led) {
                case alarm_panel::sfire: sensor="fire_";break;
                case alarm_panel::salarm: sensor="alm_";break;
                case alarm_panel::strouble: sensor="trbl_";break;
                case alarm_panel::sarmedstay:sensor="arms_";break;
                case alarm_panel::sarmedaway: sensor="arma_";break;
                case alarm_panel::sinstant: sensor="armi_";break; 
                case alarm_panel::sready: sensor="rdy_";break; 
                case alarm_panel::sac: alarm_panel::publishBinaryState("ac",0,open);return;       
                case alarm_panel::sbypass: sensor="byp_";break;  
                case alarm_panel::schime: sensor="chm_";break;
                case alarm_panel::sbat: alarm_panel::publishBinaryState("bat",0,open);return;  
                case alarm_panel::sarmednight: sensor="armn_";break;  
                case alarm_panel::sarmed: sensor="arm_";break;  
                case alarm_panel::soffline: break;       
                case alarm_panel::sunavailable: break; 
                default: break;
         };
      alarm_panel::publishBinaryState(sensor.c_str(),partition,open);
    }'''
zonebinary='''[&](int zone, bool open) {
      std::string sensor = "z" + std::to_string(zone) ;
      alarm_panel::publishBinaryState(sensor.c_str(),0,open);    
    }'''
zonestatus='''[&](int zone, std::string open) {
      std::string sensor = "z" + std::to_string(zone);
      alarm_panel::publishTextState(sensor.c_str(),0,&open); 
    }''' 
relay='''[&](uint8_t addr,int channel,bool open) {
      std::string sensor = "r"+std::to_string(addr) + std::to_string(channel);
      alarm_panel::publishBinaryState(sensor.c_str(),0,open);       
    }'''


CONFIG_SCHEMA = cv.Schema(
    {
    cv.GenerateID(): cv.declare_id(Component),
    cv.Optional(CONF_ACCESSCODE): cv.string  ,
    cv.Optional(CONF_MAXZONES): cv.int_, 
    cv.Optional(CONF_MAXPARTITIONS): cv.int_, 
    cv.Optional(CONF_RFSERIAL): cv.string, 
    cv.Optional(CONF_DEFAULTPARTITION): cv.int_, 
    cv.Optional(CONF_DEBUGLEVEL): cv.int_, 
    cv.Optional(CONF_KEYPAD1): cv.int_, 
    cv.Optional(CONF_KEYPAD2): cv.int_, 
    cv.Optional(CONF_KEYPAD3): cv.int_, 
    cv.Optional(CONF_RXPIN): cv.int_, 
    cv.Optional(CONF_TXPIN): cv.int_, 
    cv.Optional(CONF_MONITORPIN): cv.int_, 
    cv.Optional(CONF_EXPANDER1): cv.int_, 
    cv.Optional(CONF_EXPANDER2): cv.int_, 
    cv.Optional(CONF_RELAY1): cv.int_, 
    cv.Optional(CONF_RELAY2): cv.int_, 
    cv.Optional(CONF_RELAY3): cv.int_, 
    cv.Optional(CONF_RELAY4): cv.int_, 
    cv.Optional(CONF_TTL): cv.int_, 
    cv.Optional(CONF_QUICKARM): cv.boolean, 
    cv.Optional(CONF_LRR): cv.boolean, 
    cv.Optional(CONF_FAULT): cv.string  ,
    cv.Optional(CONF_BYPASS): cv.string  ,
    cv.Optional(CONF_ALARM): cv.string  ,
    cv.Optional(CONF_FIRE): cv.string  ,  
    cv.Optional(CONF_CHECK): cv.string  ,
    cv.Optional(CONF_TRBL): cv.string  ,   
    cv.Optional(CONF_HITSTAR): cv.string  ,    
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID],config[CONF_KEYPAD1],config[CONF_RXPIN],config[CONF_TXPIN],config[CONF_MONITORPIN],config[CONF_MAXZONES],config[CONF_MAXPARTITIONS])
    
    if CONF_ACCESSCODE in config:
        cg.add(var.set_accessCode(config[CONF_ACCESSCODE]));
    if CONF_MAXZONES in config:
        cg.add(var.set_maxZones(config[CONF_MAXZONES]));
    if CONF_MAXPARTITIONS in config:
        cg.add(var.set_maxPartitions(config[CONF_MAXPARTITIONS]));
    if CONF_RFSERIAL in config:
        cg.add(var.set_rfSerialLookup(config[CONF_RFSERIAL]));
    if CONF_DEFAULTPARTITION in config:
        cg.add(var.set_defaultPartition(config[CONF_DEFAULTPARTITION]));
    if CONF_DEBUGLEVEL in config:
        cg.add(var.set_debug(config[CONF_DEBUGLEVEL]));
    if CONF_KEYPAD1 in config:
        cg.add(var.set_partitionKeypad(1,config[CONF_KEYPAD1]));
    if CONF_KEYPAD2 in config:
        cg.add(var.set_partitionKeypad(2,config[CONF_KEYPAD2]));
    if CONF_KEYPAD3 in config:
        cg.add(var.set_partitionKeypad(3,config[CONF_KEYPAD3]));
    if CONF_EXPANDER1 in config:
        cg.add(var.set_expanderAddr(1,config[CONF_EXPANDER1]));
    if CONF_EXPANDER2 in config:
        cg.add(var.set_expanderAddr(2,config[CONF_EXPANDER2]));
    if CONF_RELAY1 in config:
        cg.add(var.set_expanderAddr(3,config[CONF_RELAY1]));
    if CONF_RELAY2 in config:
        cg.add(var.set_expanderAddr(4,config[CONF_RELAY2]));
    if CONF_RELAY3 in config:
        cg.add(var.set_expanderAddr(5,config[CONF_RELAY3]));
    if CONF_RELAY4 in config:
        cg.add(var.set_expanderAddr(6,config[CONF_RELAY4]));
    if CONF_TTL in config:
        cg.add(var.set_ttl(config[CONF_TTL]));        
    if CONF_QUICKARM in config:
        cg.add(var.set_quickArm(config[CONF_QUICKARM]));        
    if CONF_LRR in config:
        cg.add(var.set_lrrSupervisor(config[CONF_LRR]));      

    if CONF_FAULT in config:
        cg.add(var.set_text(1,config[CONF_FAULT])); 
    if CONF_BYPASS in config:
        cg.add(var.set_text(2,config[CONF_BYPASS])); 
    if CONF_ALARM in config:
        cg.add(var.set_text(3,config[CONF_ALARM])); 
    if CONF_FIRE in config:
        cg.add(var.set_text(4,config[CONF_FIRE])); 
    if CONF_CHECK in config:
        cg.add(var.set_text(5,config[CONF_CHECK])); 
    if CONF_TRBL in config:
        cg.add(var.set_text(6,config[CONF_TRBL]));  
    if CONF_HITSTAR in config:
        cg.add(var.set_text(7,config[CONF_HITSTAR]));         
        
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
    