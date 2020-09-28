# Honeywell/Ademco Vista ECP ESPHome custom component and library

This is an implementation of an ESPHOME custom component and ESP Library to interface directly to a Safewatch/Honeywell/Ademco Vista 15/20 alarm system using the ECP interface and very inexpensive ESP8266/ESP32 modules .  The ECP library code is based on the arduino source code from Mark Kimsal's repository located at  https://github.com/TANC-security/keypad-firmware.  It has been  completely rewritten as a class and adapted to work on the ESP8266/ESP32 platform using interrupt driven communications and pulse timing. A custom modified version of Peter Lerup's ESPsoftwareserial library (https://github.com/plerup/espsoftwareserial) was also used for the serial communications to work more efficiently within the tight timing of the ESP8266 interrupt window. 

To compensate for the limitations of the minimal zone data sent by the panel, a time to live (TTL) attribute for each faulted zone was used.  The panel only sends fault messages when a zone is faulted or alarmed and does not send data when the zone is restored, therefore the TTL timer is used to reset a zone after a preset duration once it stops receiving those fault/alarm messages for that zone.  You can tweak the TTL setting in the YAML.  The default timer is set to 30 seconds.  I've also added persistent storage and recovery for zone status in the event of a power failure or reboot of the ESP.  The system will use persistent storage to recover the last known status of the zone on restart.

From documented info, it seems that some panels send an F2 command with extra system details but the panel I have here (Vista 20P version 3.xx ADT version) does not.  Only the F7 is available for zone and system status in my case but this is good enough for this purpose. 

As far as writing on the bus and the request to send pulsing sequence, most documentation only discusses keypad traffic and this only uses the the 3rd pulse.  In actuality the pulses are used as noted below depending on the device type requesting to send:
```
Panel pulse 1. Addresses 1-7, expander board (07), etc
Panel pulse 2. Addresses 8-15 - zone expanders, relay modules
Panel pulse 3. Addresses 16-23 - keypads
```
For example, a zone expander that has the address 07, will send it's address on the first pulse only and will send nothing for the 2nd and 3rd pulse.  A keypad with address 16, will send a 1 bit pulse for pulse1 and pulse2 and then it's encoded address on pulse 3. This info was determined from analysis using a zone expander board and Pulseview to monitor the bus. 

If you are not familiar with ESPHome , I suggest you read up on this application at https://esphome.io and home assistant at https://www.home-assistant.io/.   The library class itself can be used outside of the esphome and home assistant systems.  Just use the code as is without the vistalalarm.yaml and vistaalarm.h files and call it's functions within your own application.  You can use an MQTT front end if that's your preference. I might add an example MQTT arduino ino project at a later time.

To use this software you simply place the vistaAlarm.yaml file in your main esphome directory, then copy the *.h and *.cpp files from the vistaEcpInterface directory to a similarly named subdirectory (case sensitive) in your esphome main directory and then compile the yaml as usual. The directory name is in the "includes:" option of the yaml.

The yaml attributes should be fairly self explanatory for customization. The yaml example also shows how to setup named zones. 

## Features:

* Full zone expander emulation which will give you  an additional 8 zones to the system per emulated expander. Currently the library will provide emulation for 2 boards for a total of 16 additionals zones. You can even use free pins on the chip as triggers for those zones as well. 

* Long Range Radio (LRR) emulation (or monitoring) statuses for more detailed status messages

* Zone status - Open, Busy, Alarmed and Closed with named zones

* Arm, disarm or send any sequence of commands to the panel

* Status indicators - fire, alarm, trouble, armed stay, armed away, instant armed, armed night,  ready, AC status, bypass status, chime status,battery status, check status

* Optional ability to monitor other devices on the bus such as keypads, other expanders, relay boards, RF devices, etc. This requires the #define MONITORTX to be uncommented in vista.h as well as the addition of two resistors (R4 and R5) to the circuit as shown in the schematic.   This adds another serial interupt routine that captures and decodes all data on the green tx line.  Currently this data is not used in this component but is provided for future use. For now, it will only be printed on the console. 

The following services are published to home assistant for use in various scripts. 

	alarm_disarm: Disarms the alarm with the user code provided, or the code specified in the configuration.
	alarm_arm_home: Arms the alarm in home mode.
	alarm_arm_away: Arms the alarm in away mode.
	alarm_arm_night: Arms the alarm in night mode (no entry delay).
	alarm_trigger_panic: Trigger a panic alarm.
    alarm_trigger_fire: Trigger a fire alarm.
	alarm_keypress: Sends a string of characters to the alarm system. 

## Example in Home Assistant
![Image of HASS example](https://github.com/Dilbert66/esphome-vistaECP/blob/master/vista-ha.png)

The returned statuses for Home Assistant are: armed_away, armed_home, armed_night, pending, disarmed,triggered and unavailable.  

Sample Home Assistant Template Alarm Control Panel configuration with simple services (defaults to partition 1):

```
alarm_control_panel:
  - platform: template
    panels:
      safe_alarm_panel:
        name: "Alarm Panel"
        value_template: "{{states('sensor.vistaalarm_system_status')}}"
        code_arm_required: false
        
        arm_away:
          - service: esphome.vistaalarm_alarm_arm_away
                  
        arm_home:
          - service: esphome.vistaalarm_alarm_arm_home
          
        arm_night:
          - service: esphome.vistaalarm_alarm_arm_night
            data_template:
              code: '{{code}}' #if you didnt set it in the yaml, then send the code here
          
        disarm:
          - service: esphome.vistaalarm_alarm_disarm
            data_template:
              code: '{{code}}'                    
```

## Services

- Basic alarm services. These services default to partition 1:

	- "alarm_disarm", Parameter: "code" (access code)
	- "alarm_arm_home" 
	- "alarm_arm_night", Parameter: "code" (access code)
	- "alarm_arm_away"
	- "alarm_trigger_panic"
	- "alarm_trigger_fire"


- Intermediate command service. Use this service if you need more versatility such as setting alarm states on any partition:

	- "set_alarm_state",  Parameters: "partition","state","code"  where partition is the partition number from 1 to 8, state is one of "D" (disarm), "A" (arm_away), "S" (arm_home), "N" (arm_night), "P" (panic) or "F" (fire) and "code" is your panel access code (can be empty for arming, panic and fire cmds )

- Generic command service. Use this service for more complex control:

	- "alarm_keypress",  Parameter: "keys" where keys can be any sequence of keys accepted by your panel. For example to arm in night mode you set keys to be "xxxx33" where xxxx is your access code. 
    
    - "set_zone_fault",Parameters: "zone","fault" where zone is a zone from 9 - 48 and fault is 0 or 1 (0=ok, 1=open)
       The zone number will depend on what your expander address is set to.


## Wiring


![Image of HASS example](https://github.com/Dilbert66/esphome-vistaECP/blob/master/ECPInterface.png)


## Wiring Notes
* None of the components are critical.  Any small optocoupler should be fine for U2.  You can also vary the resistor values but keep the ratio similar for the voltage dividers R2/R3 and (optional) R4/R5.  R1 should not be set below 220 ohm.  As noted, if you don't intend to use the MONITORTX function, you don't need R4/R5.  You should also be able to power via USB but I recommend using a power source that can provide at least 800ma.  You can also use those cheap buck voltage regulators for power.

## OTA updates
In order to make OTA updates, connection switch in frontend should be switched to OFF since the  ECP library is using interrupts.


## References 
You can checkout the links below for further reading and other implementation examples. Some portions of the code in the repositories below was used in creating the library.
* https://github.com/TANC-security/keypad-firmware
* https://github.com/cweemin/espAdemcoECP
* https://github.com/TomVickers/Arduino2keypad/

This project is licensed under the Lesser General Public License version 2.1, or (at your option) any later version as per it's use of other libraries and code. Please see COPYING.LESSER for more informatio



