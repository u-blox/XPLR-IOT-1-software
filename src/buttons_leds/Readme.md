# Button Functionality and LED indications
XPLR-IOT-1 comes with two buttons and an RGB LED. The source files in this folder implement their functionality. The LEDs are driven by the PWM device to allow them perform a fading effect.

## Button Functionality
After initialization the XPLR-IOT-1 does not perform any particular action. It waits commands from the user. The user can give those commands either directly using a serial terminal (this way all commands can be sent) or he can use the two buttons to perform the main two operations of the application. Those two operations are:
- Sensor Aggregation Main Function over Wi-Fi.
- Sensor Aggregation Main Function over Cellular.

### Using Button 1
Instead of sending a command the user can press Button 1 to activate Sensor Aggregation Main Function over Wi-Fi.

When button 1 is pressed the cyan LED lights up. If button 1 is pressed for about 3 seconds, the LED blinks for some time (the button should then be released). That means the command to activate the function has been received. This has the same effect as sending the command:
```
functions wifi_start
```
While the application is running, if the user presses the Button 1 again for 3 seconds (until the LED blinks), the Wi-Fi is disconnected, and the application stops. This has the same effect as sending the command:
```
functions wifi_stop
```
If button 1 is pressed while Sensor Aggregation application runs over cellular, the cellular application stops, and the Wi-Fi then starts (function swap)

**Notes**:
-	If the user has used some commands to configure sensors individually, when the sensor aggregation main function is activated, this configuration is lost, and the sensors are configured to work as the sensor aggregation main function dictates (all enabled at the same sampling period).
-	While the application initialization is in progress, the stop command won’t work.
-	While the application initializes the application cannot switch from Wi-Fi to Cellular (with a command or button press). If such request is sent, a red blink 3 times notifies the user that this action cannot be performed.

The notes above are also true for button 2

### Using Button 2
Instead of sending a command the user can press Button 2 to activate Sensor Aggregation Main Function over cellular.  

When button 2 is pressed the green LED lights up. If button 2 is pressed for about 3 seconds, the LED blinks for some time (the button should then be released). That means the command to activate the function has been received. This has the same effect as sending the command:
```
functions cell_start
```
While the application is running, if the user presses the Button 2 again for 3 seconds (until the LED blinks), the cellular is disconnected, and the application stops. This has the same effect as sending the command
```
functions cell_stop
```
If button 2 is pressed while Sensor Aggregation application runs over Wi-Fi, the Wi-Fi application stops, and the cellular then starts (function swap)

**The same notes mentioned in Button 1 functionality are also true for Button 2**


## LED Indications

In the following table the LED indications of the application are explained.
The following description of LED status is not applicable if LED commands have been issued by the user 

|LED Description|Status Indication|
|:----|:----|
|Cyan Steadily On| Button 1 is pressed or|
| | Sensor Aggregation Main Function over Wi-Fi is active|
|Cyan LED Blinks|If button 1 is pressed for ~3 seconds the LED blinks to indicate that Sensor Aggregation Main function over Wi-Fi has been activated|
|Cyan Fade In/Out (500ms in, 500ms out)|XPLR-IOT-1 tries to connect to a Wi-Fi network|
|Cyan Fade In/Out (a bit faster -  200ms in 200ms out)|XPLR-IOT-1 tries to connect to MQTT Now|
|Green Steadily On| Button 2 is pressed or|
| |Sensor Aggregation Main Function over cellular is active|
|Green LED Blinks|If button 2 is pressed for ~3 seconds the LED blinks to indicate that Sensor Aggregation Main function over cellular has been activated|
|Green Fade In/Out (500ms in, 500ms out)|XPLR-IOT-1 tries to connect to a cellular network|
|Green Fade In/Out (a bit faster ? 200ms in 200ms out)|XPLR-IOT-1 tries to connect to MQTT Anywhere (or Flex)|
|White Fade In/Out|XPLR-IOT-1 is in deinitializing procedure (Wi-Fi or cellular)|
|Red Blinks (3 times fast)|Indication of error|

**Notes**: It has been noticed the LED behavior is not very robust when a Sensor Aggregation mode is active and then a button is pressed and released (LED should remain on, but it may close). Also sometimes the blinking that indicates activation of the mode does not always work well.
