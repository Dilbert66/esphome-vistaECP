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

If you are not familiar with ESPHome , I suggest you read up on this application at https://esphome.io and home assistant at https://www.home-assistant.io/.   The library class itself can be used outside of the esphome and home assistant systems.  Just use the code as is without the vistalalarm.yaml and vistaalarm.h files and call it's functions within your own application.  

To use this software you simply place the vistaAlarm.yaml file in your main esphome directory, then copy the *.h and *.cpp files from the vistaEcpInterface directory to a similarly named subdirectory (case sensitive) in your esphome main directory and then compile the yaml as usual. The directory name is in the "includes:" option of the yaml.

##### Notes: 
* If you use the zone expanders and/or LRR functions, you might need to clear CHECK messages for the LRR and expanded zones from the panel on boot or restart by entering your access code followed by 1 twice. eg 12341 12341 where 1234 is your access code.

The yaml attributes should be fairly self explanatory for customization. The yaml example also shows how to setup named zones. 


## Features:

* Full zone expander emulation (4219/4229) which will give you  an additional 8 zones to the system per emulated expander plus associated relay outputs. Currently the library will provide emulation for 2 boards for a total of 16 additionals zones. You can even use free pins on the chip as triggers for those zones as well. 

* Relay module emulation. (4204). The system can support 4 module addresses for a total of 16 relay channels. 

* Long Range Radio (LRR) emulation (or monitoring) statuses for more detailed status messages

* Zone status - Open, Busy, Alarmed and Closed with named zones

* Arm, disarm or send any sequence of commands to the panel

* Status indicators - fire, alarm, trouble, armed stay, armed away, instant armed, armed night,  ready, AC status, bypass status, chime status,battery status, check status, zone and relay channel status fields.


* Optional ability to monitor other devices on the bus such as keypads, other expanders, relay boards, RF devices, etc. This requires the #define MONITORTX to be uncommented in vista.h as well as the addition of two resistors (R4 and R5) to the circuit as shown in the schematic.   This adds another serial interrupt routine that captures and decodes all data on the green tx line.  If enabled this data will be used to update zone statuses for external modules.

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


![image](https://user-images.githubusercontent.com/7193213/121598120-78816c00-ca0f-11eb-9240-d03ec9b9c50f.png)


## Wiring Notes
* None of the components are critical.  Any small optocoupler should be fine for U2.  You can also vary the resistor values but keep the ratio similar for the voltage dividers R2/R3 and (optional) R4/R5.  R1 should not be set below 220 ohm.  As noted, if you don't intend to use the MONITORTX function, you don't need R4/R5.  You should also be able to power via USB but I recommend using a power source that can provide at least 400ma. For external power I recommend an adjustable LM2596 or MP1584EN buck converter module to convert the 12volts to 5v or 3.3 volt. If you find that you are getting inconsistent reads for commands and modules you can change the 33k/10k ratio to increase the voltage at the esp pins by either reducing the 33k or increasing the 10k values by a few K for both panel read lines.


## OTA updates
In order to make OTA updates, connection switch in frontend should be switched to OFF since the  ECP library is using interrupts.

## MQTT with HomeAssistant
If your preference is to use MQTT instead of ESPHOME, you can use the Arduino sketch from the MQTT-Example directory. It supports pretty much all functions of the ESPHOME implementation.  To use, edit the configuration items at the top of the file for your setup then simply put the ino and all *.h and *.cpp vista library files in the same sketch directory and compile.  Read the comments within the sketch for more details.   The sketch also supports ArduinoOTA (https://www.arduino.cc/reference/en/libraries/arduinoota/) that will enable you to update the code via wifi once the initial upload is done.  

## Custom Alarm Panel Card

I've added a sample lovelace alarm-panel card copied from the repository at https://github.com/GalaxyGateway/HA-Cards. I've customized it to work with this ESP library's services.   I've also added two new text fields that will be used by the card to display the panel prompts the same way a real keypad does. To configure the card, just place the alarm-panel-card.js file into the /config/www directory of your homeassistant installation and add a new resource in your lovelace configuration pointing to /local/alarm-panel-card.js.  You can then configure the card as shown below. Just substitute your service name to your application.

```
type: 'custom:alarm-keypad-card'
title: Vista_ESPHOME
unique_id: vista1
disp_line1: sensor.vistaalarm_line1
disp_line2: sensor.vistaalarm_line2
scale: 1
service_type: esphome
service: vistaalarm_alarm_keypress
status_A: AWAY
status_B: STAY
status_C: READY
status_D: BYPASS
status_E: TROUBLE
status_F: ''
status_G: ''
status_H: ''
sensor_A: binary_sensor.vistaalarm_away
sensor_B: binary_sensor.vistaalarm_stay
sensor_C: binary_sensor.vistaalarm_ready
sensor_D: binary_sensor.vistaalarm_bypass
sensor_E: binary_sensor.vistaalarm_trouble
button_A: STAY
button_B: AWAY
button_C: DISARM
button_D: BYPASS
button_F: <
button_G: '>'
button_E: ' '
button_H: ' '
cmd_A:
  keys: '12343'
cmd_B:
  keys: '12342'
cmd_C:
  keys: '12341'
cmd_D:
  keys: '12346#'
cmd_E:
  keys: ''
cmd_H:
  keys: ''
cmd_F:
  keys: <
cmd_G:
  keys: '>'
key_0:
  keys: '0'
key_1:
  keys: '1'
key_2:
  keys: '2'
key_3:
  keys: '3'
key_4:
  keys: '4'
key_5:
  keys: '5'
key_6:
  keys: '6'
key_7:
  keys: '7'
key_8:
  keys: '8'
key_9:
  keys: '9'
key_star:
  keys: '*'
key_pound:
  keys: '#'
key_right:
  keys: '>'
key_left:
  keys: <
beep: sensor.vistaalarm_beeps
view_pad: true
view_display: true
view_status: true
view_status_2: true
view_bottom: true


 
type: 'custom:alarm-keypad-card'
title: Vista_MQTT
unique_id: vista2
disp_line1: sensor.displayline1
disp_line2: sensor.displayline2
scale: 1
service_type: mqtt
service: publish
status_A: AWAY
status_B: STAY
status_C: READY
status_E: BYPASS
status_D: TROUBLE
status_F: ''
status_G: ''
status_H: ''
sensor_A: sensor.vistaaway
sensor_B: sensor.vistastay
sensor_C: sensor.vistaready
sensor_E: sensor.vistabypass
sensor_D: sensor.vistatrouble
button_A: STAY
button_B: AWAY
button_C: DISARM
button_D: BYPASS
cmd_A:
  topic: vista/Set/Cmd
  payload: '!12343'
cmd_B:
  topic: vista/Set/Cmd
  payload: '!12342'
cmd_C:
  topic: vista/Set/Cmd
  payload: '!12341'
cmd_D:
  topic: vista/Set/Cmd
  payload: '!12346#'
key_0:
  topic: vista/Set/Cmd
  payload: '!0'
key_1:
  topic: vista/Set/Cmd
  payload: '!1'
key_2:
  topic: vista/Set/Cmd
  payload: '!2'
key_3:
  topic: vista/Set/Cmd
  payload: '!3'
key_4:
  topic: vista/Set/Cmd
  payload: '!4'
key_5:
  topic: vista/Set/Cmd
  payload: '!5'
key_6:
  topic: vista/Set/Cmd
  payload: '!6'
key_7:
  topic: vista/Set/Cmd
  payload: '!7'
key_8:
  topic: vista/Set/Cmd
  payload: '!8'
key_9:
  topic: vista/Set/Cmd
  payload: '!9'
key_star:
  topic: vista/Set/Cmd
  payload: '!*'
key_pound:
  topic: vista/Set/Cmd
  payload: '!#'
view_pad: true
view_display: true
view_status: true
view_status_2: true
view_bottom: false
beep: sensor.vistabeeps

```
![image](https://user-images.githubusercontent.com/7193213/117702822-052dd580-b197-11eb-90a8-9232d6561ecf.png)



### sample sensor configuration for card using mqtt
```

sensor:

  - platform: mqtt
    state_topic: "vista/Get/DisplayLine/1"
    name: "DisplayLine1"

  - platform: mqtt
    state_topic: "vista/Get/DisplayLine/2"
    name: "DisplayLine2"

  - platform: mqtt
    state_topic: "vista/Get/Status/AWAY"
    name: "vistaaway"
    
  - platform: mqtt
    state_topic: "vista/Get/Status/STAY"
    name: "vistastay"
  
  - platform: mqtt
    state_topic: "vista/Get/Status/READY"
    name: "vistaready"

  - platform: mqtt
    state_topic: "vista/Get/Status/TROUBLE"
    name: "vistatrouble"

  - platform: mqtt
    state_topic: "vista/Get/Status/BYPASS"
    name: "vistabypass"
    
  - platform: mqtt
    state_topic: "vista/Get/Status/CHIME"
    name: "vistachime"    

```

## References 
You can checkout the links below for further reading and other implementation examples. Some portions of the code in the repositories below was used in creating the library.
* https://github.com/TANC-security/keypad-firmware
* https://github.com/cweemin/espAdemcoECP
* https://github.com/TomVickers/Arduino2keypad/

This project is licensed under the Lesser General Public License version 2.1, or (at your option) any later version as per it's use of other libraries and code. Please see COPYING.LESSER for more informatio



