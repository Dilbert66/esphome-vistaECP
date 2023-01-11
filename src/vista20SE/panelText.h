#ifndef __PANELTEXT_H
#define __PANELTEXT_H

 //EN
 
    //lookups for determining zone status as strings.  Must include complete word before zone#. No spaces.
    
    const char * FAULT = "FAULT";    
    const char * BYPAS = "BYPAS";
    const char * ALARM = "ALARM";
    const char * FIRE = "FIRE";
    const char * CHECK = "CHECK";
    
    //Can contain any substring found in the panel message.
    const char * HITSTAR = "Hit *";      
    

 //messages to display to home assistant

    const char *
      const STATUS_ARMED = "armed_away";
    const char *
      const STATUS_STAY = "armed_home";
    const char *
      const STATUS_NIGHT = "armed_night";
    const char *
      const STATUS_OFF = "disarmed";
    const char *
      const STATUS_ONLINE = "online";
    const char *
      const STATUS_OFFLINE = "offline";
    const char *
      const STATUS_TRIGGERED = "triggered";
    const char *
      const STATUS_READY = "ready";
      
    //the default ha alarm panel card likes to see "unavailable" instead of not_ready when the system can't be armed
    const char *
      const STATUS_NOT_READY = "unavailable";
    const char *
      const MSG_ZONE_BYPASS = "zone_bypass_entered";
    const char *
      const MSG_ARMED_BYPASS = "armed_custom_bypass";
    const char *
      const MSG_NO_ENTRY_DELAY = "no_entry_delay";
    const char *
      const MSG_NONE = "no_messages";
    
#endif
    