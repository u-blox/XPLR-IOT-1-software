# Shell Commands
The firmware includes a shell which can be used to control the device where the user can explore and send commands. A serial terminal program can be used to send commands and obtain log messages from the Device. **Tera Term is recommended.**

In order to use the shell the serial terminal programmed should be connected to XPLR-IOT-1 **Interface 0** with the following parameters:
- Baud rate = 115200 bps 
- Parity = none 
- Data bits = 8 
- Stop bits = 1 
- Flow control = none


![usbhub.png should be here.](../../readme_images/usbhub.png "usbhub.png image")

If these devices are not visible/available in the device manager maybe drivers need to be installed. They can be found [here](https://www.silabs.com/documents/public/software/CP210x_Windows_Drivers.zip )

After the device it reset, some initialization information is typed in the terminal and a prompt for command input appears. The commands are structured in levels. For example “led” command in level 0, has some sub-commands such as “on”,”off”,”blink” at level 1 etc. 
By pressing Tab, the command options are shown at each level. Typing “help” provides a list of the available commands at each level.

There are four main command categories:
-	**functions:** These are commands which control the Sensor Aggregation Main Functionality
-	**led :** Contains command to control the LED
-	**sensors :** Contains commands to control the sensors and their options within the Sensor Aggregation use context.
-	**modules :** Contains commands to control the u-blox modules of XPLR-IOT-1. These are the NINA-W156, SARA-R5 and MAXM10S. These commands also are used to control MQTT and MQTT-SN options/configutation as the native MQTT(SN) clients of these modules are used (the client is not implemented in the firmware but the firmware just sends the appropriate commands to the u-blox modules, which then use their internally implemented MQTT client to perform any work).
-	**version:** Types the firmware version


There are also two commands, which are not part of the sensor aggregation firmware but are implemented by Zephyr:
-	**help:** provides helpful information about the usage of shell and the commands
-	**log:** command used to control the log messages. Every module can have its messages turned on/off and there are certain levels of log messages which also can be controlled. Messages produces by ubxlib cannot be controlled by this command, but through ubxlib itself.

#### LED commands

These commands can control the LED to try various patterns. The commands provided are described below.

Where <color> is mentioned, that can be:
-	red
-	green
-	blue
-	yellow
-	purple
-	cyan
-	white

|Command|Command example|Description|
|:----|:----|:----|
|led off|led off|This command immediately turns off the LED and terminates any pending blinking/fading|
|led on <color> |led on red |Opens the LED indefinitely until another LED command is issued or the device should be indicated using a LED (e.g., a button is pressed)|
|led blink <color> <delay on> <delay off> <optional blinks>|led blink red 100 100 3      led blink yellow 500 100|LED starts to blink with an on/off period as requested. If blinks parameter is given it blinks for <blinks> times, otherwise it blinks indefinitely until another command is issued or a device status should be indicated by LED|
|led fade <color> <delay on> <delay off> <optional blinks>|led fade red 100 100 3    led fade yellow 500 100|Same as blink command but instead of rapidly blinking the LED fades in or out for the requested on/off time|

**Note:** LED commands should not be issued while the device is trying to connect to a network (module commands have been issued and are still in progress) or when Sensor Aggregation Main Functionality is active or buttons are pressed (see 1.1) Those actions may request access to the LED to indicate a status and could lead to unpredictable behavior of the LED.

#### Functions commands
These commands control the Sensor Aggregation Main Functionality.
This function establishes a connection to Thingstream and starts sampling the sensors and send their data. 
The exact flow of the function and the messages format are described in the respective folders

|Command|Command example|Description|
|:----|:----|:----|
|functions status|functions status|Reports back to terminal if the function is active and the setting of the sampling period. <br/> The status can be:  -Disabled / -WiFi / -Cell .  The status changes once the requested operation (WiFi, Cell) has been activated successfully. While the operation is still in progress (e.g. WiFi tries to connect) the status seems disabled|
|functions set_period <period in milliseconds>|functions set_period 10000|Sets the sampling period of the function. This command can be used only if the function is currently disabled. If it is active the access to this command is denied. So  if the user wants to change the period, he should disable the function (if active), then send this command to change the sampling period and then re-activate the function.|
|functions wifi_start|functions wifi_start|This command starts the sensor aggregation function using wi-fi. If setup successfully the device will start sending sampling data via Wi-Fi at the requested period (the period can be checked with the status command) and function status will update. If the setup fails, the device will try to reverse any configuration performed. The status will not update. |
|functions wifi_stop|functions wifi_stop|If the wifi sensor aggregation function is active, this command deactivates it and stops the function.|
|functions cell_start|functions cell_stop|Same as functions wifi_start, but for cellular connection|
|functions cell_stop|functions cell_stop|Same as functions wifi_stop, but for cellular connection|

#### Sensor commands

These commands should be used only when the Sensor Aggregation Main Functionality is disabled.
They control each sensor independently. In contrast the Sensor Aggregation Main Functionality applies the same parameters (e.g. sampling rate) to all sensors. That is why most of these commands are locked when the Sensor Aggregation Main Functionality is active.
There are commands which apply to all sensors at once and commands that apply to each sensor independently.
The commands that apply **to all sensors** are described below

|Command|Command example|Description|
|:----|:----|:----|
|sensors status|sensors status|This command provides information about the status of all sensors (including information of MAXM10S which is considered both as module and sensor). Each sensor is indicated as ok or not ok if it has been initialized properly. If a sensor is not ok this will not change until the device is reset (this status cannot change during runtime, only during initialization)|
|sensors enable <all/none>|sensors enable all / sensors enable none|Enables all sensors/ disables all sensors.This enables sensor measurements. Those measurements can be typed in log messages. The fact they are enabled does not mean that these measurements will also be published to Thingstream|
|sensors publish <all/none>|sensors publish all /sensors publish none|Enables publish of all sensor measurements. Disables publish of all measurements. See publish command description below|

Independent sensor commands are given below with sensor BME280 as example:

|Command|Command example|Description|
|:----|:----|:----|
|sensors BME280 enable|sensors BME280 enable|Enables the sensor measurements. It basically enables the thread of the sensor. This thread samples the sensor with the requested sampling rate and if publish for this sensor is enabled, tries to publish those measurements to Thignstream|
|sensors BME280 disable|sensors BME280 disable|Disables the sensor measurements. Stops the sensor?s thread and no measurements for this sensor are performed|
|sensors BME280 set_period <period in milliseconds>|Sensors BME280 set_period 10000|Sets the sampling period of the sensor. It can be applied when the sensor is enabled but will take effect after the next measurement (with the previous sampling rate). If you want for the new sampling period to take immediate effect, disable the sensor, set its period, and enable it again |
|sensors BME280 publish <on/off>|sensors BME280 publish on / sensors BME280 publish off|Disables/Enables publish of this sensor measurements. The sensor can be enabled but its measurements are not published until this command sets its publish attribute to on. Setting the attribute to on, does not mean the measurements are sent to Thingstream. An active cell or wifi connection should be enabled (see module commands). If this is not the case the setting is still valid. As soon as a connection to Thingstream is established the sensor will start sending its measurements immediately if publish attribute is enabled.|

#### Module commands

##### MAXM10S commands
These commands control MAXM10S GNSS positioning module. Most commands are accessible only if Sensor Aggregation Main Functionality is not currently active

|Command|Command example|Description|
|:----|:----|:----|
|modules MAXM10S power_on|modules MAXM10S power_on|Just powers up the module. During initialization the module is powered up by default|
|modules MAXM10S power_off|modules MAXM10S power_off|Powers off the module.|
|modules MAXM10S enable|modules MAXM10S enable|Activate the thread of the module to obtain the position at the given sampling rate. This command also powers on the module if not already powered on
|modules MAXM10S disable|modules MAXM10S disable|Disables the module's thread and stops obtaining the position. The module is not powered off though with the use of this command
|modules MAXM10S set_period <milliseconds>|modules MAXM10S set_period 10000|Sets the sampling rate of the position request (while the module is enabled)|
|modules MAXM10S set_timeout <milliseconds>|modules MAXM10S set_timeout 10000|Set the timeout period for the position request. The firmware asks the MAXM10S for its position. The module should respond within this timeout period otherwise the request is aborted. The timeout should be smaller than the sampling period|
|modules MAXM10S comm=nora|modules MAXM10S comm=nora|Conects the MAXM10S UART serial output to NORA-B1 (which runs this firmware). This is essential in order for the firmware to obtain the position and generally control the MAX modulemodules MAXM10S comm=usb|modules MAXM10S comm=usb|Used only if the user want to connect to the UART of MAXM10S through a terminal from his PC, or if he wants to use MAXM10S with u-center. If the usb comm is selected the firmware cannot control MAXM10S or get position readings (it can only power it on/off)|
|modules MAXM10S publish <on/off>|modules MAXM10S publish on / modules MAXM10S publish off|Same as the sensor publish commands|

##### NINA-W156 commands
Commands to handle NINA-W156
|Command|Command example|Description|
|:----|:----|:----|
|modules NINAW156 power_on|modules NINAW156 power_on|Just powers up the module. During initialization the module is powered off by default|
|modules NINAW156 power_off|modules NINAW156 power_off|Powers off the module.|
|modules NINAW156 init|modules NINAW156 init|Initializes ubxlib to use NINAW156 and prepares the module to connect to a WiFi network. A WiFi network (SSIS and password) should be saved in the device in order for this command to work otherwise an error will be returned (see provision command)|
|modules NINAW156 deinit|modules NINAW156 deinit|Deinitializes ubxlib and removes the module and the network (does not erase the WiFi credentials from the device)|
|modules NINAW156 connect|modules NINAW156 connect|Connects to the network saved in the memory of the device (see  provision command)|
|modules NINAW156 disconnect|modules NINAW156 disconnect|Disconnects from a wifi network|
|modules NINAW156 provision "SSID" "optional:password"|modules NINAW156 provision open_network_name/ modules NINAW156 provision net_name pass12323|The command to provide the SSID and password of a WiFi network. If a password is not provided, the network is considered an open network|
|modules NINAW156 type_cred|modules NINAW156 type_cred|Types the saved and active WiFi network credentials saved|

##### SARA-R5 commands
Commands to handle SARA-R5
|Command|Command example|Description|
|:----|:----|:----|
|modules SARAR5 power_on|modules SARAR5 power_on|Just powers up the module. During initialization the module is powered off by default|
|modules SARAR5 power_off|modules SARAR5 power_off|Powers off the module.|
|modules SARAR5 init|modules SARAR5 init|Initializes ubxlib to use SARAR5 and prepares the module to connect a Mobile operator Network. The first time additional configuration may be needed via m-center.
|modules SARAR5 deinit|modules SARAR5 deinit|Deinitializes ubxlib and removes the module from ubxlib context. Also used as a disconnect command|
|modules SARAR5 connect|modules SARAR5 connect|Connects to the network. The first time additional configuration may be needed via m-center|
|modules SARAR5 plans "param"|modules SARAR5 plans anywhere / modules SARAR5 plans flex / modules SARAR5 plans get_active|Conects the MAXM10S uart serial output to NORA-B1 (which runs this firmware). This is essential in order for the firmware to obtain the position and generally control the MAX module|

##### MQTT commands

These commands control the MQTT client of NINAW156 and are used to connect to MQTT Now in Thingstream platform. 

|Command|Command example|Description|
|:----|:----|:----|
|modules MQTT open|modules MQTT open|Needs a wifi active connection. If this does not exist it will try to connect to a Wifi network. Opens an MQTT client in a ubxlib context and prepares the module to connect. Before use the first time, MQTT save should be used to save the Thingstream platform credentials (see respective chapter)|
|modules MQTT connect|modules MQTT connect|Connects to Thingstream via MQTT (see open command)|
|modules MQTT close|modules MQTT close|Closes the MQTT client in the ubxlib context (also disconnects from Thingstream)|
|modules MQTT save "Client ID" "Username" "Password"|modules MQTT save device:2323 username passdffjh33|Command to be used before the first attempt to connect via MQTT to Thingstream. Provide thingstream portal credentials via this command. The credentials are saved to memory so the user won’t have to re-enter them each time the device resets|
|modules MQTT type|modules MQTT type|Type MQTT credentials stored in memory|
|modules MQTT status|modules MQTT status|Type information about the status of the MQTT client from a firmware (ubxlib) perspective|
|modules MQTT send "topic_name" "message" "QOS"|"QOS" = 0,1,2 (Quality of service) / modules MQTT send test_topic hello_msg 0 |Send a message to Thingstream, at any topic with the requested Quality of Service|

##### MQTTSN commands
These commands control the MQTT-SN client of SARAR5 and are used to connect to MQTT Anywhere (or Flex) in Thingstream platform. 

|Command|Command example|Description|
|:----|:----|:----|
|modules MQTTSN open|modules MQTTSN open|Needs a Cellular active connection. If this does not exist, it will try to connect to a Cellular network. Prepares the device to connect to Thingstream depending on selected plan (Anywhere  or Flex – see “modules SARAR5 plans” command)|
|modules MQTTSN connect|modules MQTTSN connect|Connects to Thingstream via MQTT (see open command)|
|modules MQTTSN close|modules MQTTSN close|Closes the MQTTSN client|
|modules MQTTSN disconnect|modules MQTTSN disconnect|Disconnects the MQTTSN client|
|modules MQTTSN save "Plan" "Device ID" "Connection Duration in seconds: if anywhere plan is selected "|modules MQTTSN save anywhere device2834762 600 / modules MQTTSN save flex device2834762|Command to be used before the first attempt to connect via MQTTSN to Thingstream. Used to provide the credentials from thingstream portal to the device|
|modules MQTT send "type" "topic" "message" "QOS"|"QOS" = 0,1,2,3 (Quality of service) / "type"=normal,short,pre (pre stands for predefined) // modules MQTTSN send pre 501 hello_msg 0|Send a message to Thingstream, at a topic with the requested Quality of Service The type is according to MQTTSN types for topics|
|modules MQTTSN status|modules MQTTSN status|Type information about the status of the MQTTSN client from a firmware (ubxlib) perspective|
|modules MQTTSN type|modules MQTTSN type|Type MQTTSN credentials stored in memory|
