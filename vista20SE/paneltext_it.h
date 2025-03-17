#ifndef __PANELTEXT_H
#define __PANELTEXT_H

 //IT
 
    //lookups for determining zone status as strings.  Must include complete word before zone#. No spaces.
    const char *const FAULT = "APERT";
    const char *const BYPAS = "ESCL.";
    const char *const ALARM = "ALARM";
    const char *const FIRE = "INCEND"; 
    const char *const CHECK = "VERIF";
    const char *const ARMED = "INSERIM."; // 25IT is like Vista20SE - ignore this
    
    //Can contain any substring found in the panel message.
    const char *const HITSTAR = "Prem";
   
    
    /*
    //alternative lookups as character arrays
    //find the matching characters in an ascii chart for the messages that your panel sends
    //for the statuses below. Only need the first few characters plus a zero at the end.
    //NOTE:  *** do NOT include the zone#. 
    const char FAULT[6] = {70,65,85,76,84,0}; //"FAULT"
    const char BYPAS[6] = {66,89,80,65,83,0}; //"BYPASS"
    const char ALARM[6] = {65,76,65,82,77,0}; //"ALARM"
    const char FIRE[6]  = {70,73,82,69,32,0}; //"FIRE "
    const char CHECK[6] = {67,72,69,67,75,0}; //"CHECK"
    const char HITSTAR[6] = {72,105,116,32,42,0}; //  "Hit *";
    */
 

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
    