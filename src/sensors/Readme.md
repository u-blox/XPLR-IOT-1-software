# Sensors
This folder contains the source files for handling the sensors within the application. The source files named **common** define some functions that include all sensors and define some types common to all sensors used by other modules in the application.

Sensors can be used in two modes:
1. Sensor Aggregation Main Functionality
1. Sensor Aggregation Custom Functionality

When mode 1 is used all sensors are enabled by default and set to the same smapling period (this period is customizeable but is the same for all sensors)

When mode 2 is used the user can use shell commands to enable each sensor separately, define different sampling periods for the sensors and decide whether he wants to publish their data or not.

**These commands are locked when mode 1 is active**

###	Handling Sensors

The basic commands of each sensor are:
-	Enable
-	Disable
-	Set period
-	Publish

The enable/disable and publish commands can be given for each sensor separately all to all sensors at once. 

##### Enable/Disable
Each sensor has a thread assigned to it. When this thread is active the sensor is sampled at a given update period. The enable/ disable command essentially activates or pauses this thread.

Each sensor is initialized and tested at startup. If the sensor is not ok, at each sampling period an error message will appear that the sensor was not read properly. If the sensor is not ok, this status probably won’t change until next startup/reset of XPLR-IOT-1.

The command is locked when Sensor Aggregation Main Function is activated.
##### Set Period
This command sets the sampling period of the sensor. When the Sensor Aggregation Main Function is not active, it can be used at any time even if the sensor is already enabled. In this case the update period will change at next sensor sampling.

*Example given:* Sensor samples every 10 seconds. At second 3 the set period command is sent to change the period to 1 second. Seven seconds should pass for the sensor to be sampled (the previous update period was 10) and then the period is updated to 1 second. If the update period should be changed immediately, then the sensor should be disabled, set its period and then enabled again. In this case the update period takes effect immediately.

Each sensor in this mode can have different update periods.
The command is locked when Sensor Aggregation Main Function is activated.

##### Publish
This sensor controls if the measurements of the sensor will be published in Thingstream or not when the XPLR-IOT-1 is connected to Thingstream. That means that the command itself does not connect the device to Thingstream, it just says that when the device connects to Thingstream, and If the sensor is enabled, every time the sensor is sampled – publish the measurement data.

The command is locked when Sensor Aggregation Main Function is activated.
So, to publish data from a sensor three things should happen:
-	The sensor is enabled
-	The sensor is set to publish its data
-	There should be an active Thingstream connection (Wi-Fi or Cellular)

These three conditions are separate from each other in Sensor Aggregation custom mode.
In this mode, each sensor sends its data to a different topic and the message format is slightly different from Sensor Aggregation Main function mode (see [data handling](../data_handle/) )


