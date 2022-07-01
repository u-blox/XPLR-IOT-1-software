# Data Handling

The application has two modes of operation:
1. Sensor Aggregation Main Functionality
1. Sensor Aggregation Custom Functionality

**Note:** The topics, measurements names and sensor IDs described below are all defined and can all be changed in **x_data_handle.h** file. However if you change those names the Node-Red Dashboard provided with this firmware application example will stop working (unless modified accordingly)

## Sensor Aggregation Main Functionality
This is the main function of the sensor aggregation firmware. In this mode the device will try to connect to a Wi-Fi or Cellular network, it will then setup MQTT (or MQTT-SN) and will try to connect to the Thingstream platform and after that it will enable all sensors and will sample all of them with a common update period. The sampling period can be configured with the appropriate shell command when the mode is not active.

In this mode all sensor data are published in a single message at a single topic (That is why all sensors are forced to have the same sampling period).
### Main Functionality Message Format
In the Sensor Aggregation Main Function mode, the device will publish all sensor data in one topic, in one message per update (all sensors have the same sampling period).
The message is the same either when sent via Wi-Fi or Cellular.

##### Topic
In this mode the message is sent at the following topic in Thingstream portal:
-	Topic Name: **C210 Sensor Aggregation**
-	Topic Path: **c210/all**
-	Topic Alias: **500**
-	Description: Contains a JSON packet of all collected sensor data during one sampling period. The JSON packet is sent as an encoded string to Base64.

**Note:** This topic should be created in Thingstream portal, before trying to send data via Cellular (it should be created automatically when the redemption code is used) 

##### JSON packet
As mentioned in the description of the topic, the message is a JSON packet which holds the sensor measurements.

An example message is given below:
```
{"Dev":"C210","Sensors":[{"ID":"BME280","mes":[{"nm":"Tm","vl":27.780},{"nm":"Hm","vl":43.391},{"nm":"Pr","vl":99.147}]},{"ID":"FXAS21002","mes":[{"nm":"Gx","vl":-0.010},{"nm":"Gy","vl":0.002},{"nm":"Gz","vl":0.002}]},{"ID":"LIS2DH12","mes":[{"nm":"Ax","vl":-0.115},{"nm":"Ay","vl":9.882},{"nm":"Az","vl":0.192}]},{"ID":"LIS3MDL","mes":[{"nm":"Mx","vl":-1.457},{"nm":"My","vl":-0.489},{"nm":"Mz","vl":-0.165}]},{"ID":"LTR303","mes":[{"nm":"Lt","vl":0}]},{"ID":"BQ27421","mes":[{"nm":"Volt","vl":4.169},{"nm":"SoC","vl":100.000}]},{"ID":"ADXL345","mes":[{"nm":"Ax","vl":-93.000},{"nm":"Ay","vl":906.000},{"nm":"Az","vl":31.000}]},{"ID":"MAXM10","mes":[{"nm":"Px","vl":38.0487025},{"nm":"Py","vl":23.8090018}]}]}
```
 If someone can copy/paste the above message to a JSON parser the structure of the message should be obvious:
-	The “Dev” indicates the device, which in our case is “C210” (C210 is the circuit board in XPLR-IOT-1)
-	The “Sensors” key indicates the start of a list of sensors of the device
Each sensor in the list is described as follows:
-	By the sensor “ID”: This contains the sensor Name (e.g., “BME280”)
-	By the sensor measurements. The start of this list is indicated be the key “mes” (stands for measurements)
Each measurement in the measurements list is described as follows:
-	By the measurement name, indicated by the key “nm”
-	By the measurement value, indicated by the key “vl”
-	In case of error, instead of the value the key “err” will appear with a value indicating the nature of the error

Each sensor is described by its name in the field “ID” – e.g., “ID”:” BME280” is BME280 environmental sensor
Each sensor can have up to 3 measurements in the list “mes”. In this list each measurement has a name “nm” and a value “vl”.

A JSON example message of a simple sensor is the following:
```
{"ID":"BME280",“mes":[{“nm":"Tm",“vl":29.520},{“nm":"Hm",“vl":28.334},{“nm":"Pr",“vl":98.990}]}
```

The message above means:
-	This is sensor BME280
-	The measurement list contains 3 measurements
--	Measurement Tm (temperature) with value 29.520 Celsius
--	Measurement Hm (humidity) with value 28.334 
--	Measurement Pr (Pressure) with value 98.990

The measurement names (the possible values of key: “nm”) are given below

|Measurement Type |Measurement Name|
|:----|:----|
|Accelerometer X|Ax|
|Accelerometer Y|Ay|
|Accelerometer Z|Az|
|Gyroscope X|Gx|
|Gyroscope Y|Gy|
|Gyroscope Z|Gz|
|Magnetometer X|Mx|
|Magnetometer Y|My|
|Magnetometer Z|Mz|
|Ambient Temperature|Tm|
|Humidity|Pr|
|Pressure|Hm|
|Battery Voltage|Volt|
|Battery State of Chare|SoC|
|Light|Lt|
|Position X|Px|
|Position Y|Py|

The sensor IDs (the possible values of key “ID”) are given below. The following table also shows the measurements each sensor can contain.

|Sensors ID|Sensor Type|Has Measurements|
|:----|:----|:----|
|BME280|Environmental Sensor|Tm, Pr, Hm|
|ADXL345|Accelerometer|Ax,Ay,Az|
|LIS2DH12|Accelerometer|Ax,Ay,Az|
|LTR303|Light|Lt|
|FXAS21002|Gyroscope|Gx,Gy,Gz|
|LIS3MDL|Magnetometer|Mx, My, Mz|
|MAXM10|GNSS/Position|Px,Py|
|BQ27421|Battery Fuel Gauge|Volt, SoC|

##### Errors
Some sensor measurements might not be available at any given sampling period (broken sensor, could not get value etc.). In that case the measurement list won’t be sent. Instead, an error key will be sent which will contain a string describing the error. Below a JSON string example of a sensor with an error is given.
```
{"ID":"MAXM10","err":"timeout"}
```
Instead of the measurement list the error is sent in the key “err”. 
In this case the GNSS position has not been obtained yet and a timeout error is sent.

Three error string are supported. These are:
- **“ok”**   : this normally should never be sent. It means no error
- **“init”** : There was an error initializing the sensor. Until reset of the device this won’t change during runtime
- **“fetch”**: There was an error while fetching the measurements from the sensor
- **“timeout”**: The measurements were not obtained in time from the sensor. This should be expected for GNSS position, until the module obtains a proper position fix. 

**Note:** Key names and error strings should be kept as short as possible. The reason for this is that in the Sensor Aggregation Main function mode the JSON strings can become large. This is getting worse by the fact that the strings are encoded to Base64 before sending. 
If the message gets bigger than 1024 characters, it won't be supported by the MQTT(SN) client.

#####  Base 64 Encoding
After the preparation of the JSON packet which contains the sensor measurements the message is Base64 encoded.

The reason for the Base64 encoding is that the JSON packet requires some double quotes “in its format. These double quotes when sent from the cellular module are aborted and that breaks the JSON format. To get around that issue the JSON string is encoded to Base64
So, the message received in Thingstream portal is something like:
```
eyJEZXYiOiJDMjEwIiwiU2Vuc29ycyI6W3siSUQiOiJCTUUyODAiLCJtZXMiOlt7Im5tIjoiVG0iLCJ2bCI6MjcuNzgwfSx7Im5tIjoiSG0iLCJ2bCI6NDMuMzkxfSx7Im5tIjoiUHIiLCJ2bCI6OTkuMTQ3fV19LHsiSUQiOiJGWEFTMjEwMDIiLCJtZXMiOlt7Im5tIjoiR3giLCJ2bCI6LTAuMDEwfSx7Im5tIjoiR3kiLCJ2bCI6MC4wMDJ9LHsibm0iOiJHeiIsInZsIjowLjAwMn1dfSx7IklEIjoiTElTMkRIMTIiLCJtZXMiOlt7Im5tIjoiQXgiLCJ2bCI6LTAuMTE1fSx7Im5tIjoiQXkiLCJ2bCI6OS44ODJ9LHsibm0iOiJBeiIsInZsIjowLjE5Mn1dfSx7IklEIjoiTElTM01ETCIsIm1lcyI6W3sibm0iOiJNeCIsInZsIjotMS40NTd9LHsibm0iOiJNeSIsInZsIjotMC40ODl9LHsibm0iOiJNeiIsInZsIjotMC4xNjV9XX0seyJJRCI6IkxUUjMwMyIsIm1lcyI6W3sibm0iOiJMdCIsInZsIjowfV19LHsiSUQiOiJCUTI3NDIxIiwibWVzIjpbeyJubSI6IlZvbHQiLCJ2bCI6NC4xNjl9LHsibm0iOiJTb0MiLCJ2bCI6MTAwLjAwMH1dfSx7IklEIjoiQURYTDM0NSIsIm1lcyI6W3sibm0iOiJBeCIsInZsIjotOTMuMDAwfSx7Im5tIjoiQXkiLCJ2bCI6OTA2LjAwMH0seyJubSI6IkF6IiwidmwiOjMxLjAwMH1dfSx7IklEIjoiTUFYTTEwIiwibWVzIjpbeyJubSI6IlB4IiwidmwiOjM4LjA0ODcwMjV9LHsibm0iOiJQeSIsInZsIjoyMy44MDkwMDE4fV19XX0=
```
The above message, when Base64 decoded, results in:

```
{"Dev":"C210","Sensors":[{"ID":"BME280","mes":[{"nm":"Tm","vl":27.780},{"nm":"Hm","vl":43.391},{"nm":"Pr","vl":99.147}]},{"ID":"FXAS21002","mes":[{"nm":"Gx","vl":-0.010},{"nm":"Gy","vl":0.002},{"nm":"Gz","vl":0.002}]},{"ID":"LIS2DH12","mes":[{"nm":"Ax","vl":-0.115},{"nm":"Ay","vl":9.882},{"nm":"Az","vl":0.192}]},{"ID":"LIS3MDL","mes":[{"nm":"Mx","vl":-1.457},{"nm":"My","vl":-0.489},{"nm":"Mz","vl":-0.165}]},{"ID":"LTR303","mes":[{"nm":"Lt","vl":0}]},{"ID":"BQ27421","mes":[{"nm":"Volt","vl":4.169},{"nm":"SoC","vl":100.000}]},{"ID":"ADXL345","mes":[{"nm":"Ax","vl":-93.000},{"nm":"Ay","vl":906.000},{"nm":"Az","vl":31.000}]},{"ID":"MAXM10","mes":[{"nm":"Px","vl":38.0487025},{"nm":"Py","vl":23.8090018}]}]}
```
Which is the JSON packet containing the measurements.

## Sensor Aggregation Custom Functionality
In the Sensor Aggregation Custom function mode, each sensor publishes its data to a separate topic. This allows for different sampling periods per sensor. 

The sensor IDs (their names) and the measurement names are the same as in Sensor Aggregation Main function mode. The difference is that each sensor has its own topic and that each message contains only one sensor and not a list of sensor measurements.

The data, again, are sent in a JSON format and encoded to Base64 as in the following example:
```
{"ID":"BME280",“mes":[{“nm":"Tm",“vl":29.520},{“nm":"Hm",“vl":28.334},{“nm":"Pr",“vl":98.990}]}
```
This is a message for the BME280 sensor which should be published to the BME280 topic.

The topics, aliases etc. are presented in the table that follows
|Sensors ID|Topic Name|Topic Path|Topic alias|Example Message String (decoded)|
|:----|:----|:----|:----|:----|
|BME280|Environmental Measurements|c210/sensor/environmental|501|{"ID":"BME280","mes":[ {"nm":"Tm","vl":29.520},{"nm":"Hm","vl":28.334},{"nm":"Pr","vl":98.990}]}|
|BQ27421|Battery Measurements|c210/sensor/battery|502|{"ID":"BQ27421","mes":[ {"nm":"Volt","vl":4.1},{"nm":"SoC","vl":100}]}|
|ADXL345|Accelerometer ADXL345 measurements|c210/sensor/accelerometer/adxl345|503|{"ID":"ADXL345","mes":[ {"nm":"Ax","vl":25.33333},{"nm":"Ay","vl":67.55333},{"nm":"Az","vl":33.44333}]}|
|LIS2DH12|Accelerometer LIS2DH12 measurements|c210/sensor/accelerometer/is2dh12|504|{"ID":"LIS2DH12","mes":[ {"nm":"Ax","vl":25.33333},{"nm":"Ay","vl":67.55333},{"nm":"Az","vl":33.44333}]}|
|LIS3MDL|Magnetometer LIS3MDL measurements|c210/sensor/magnetometer|505|{"ID":"LIS3MDL","mes":[ {"nm":"Mx","vl":25.33333},{"nm":"My","vl":67.55333},{"nm":"Mz","vl":33.44333}]}|
|LTR303|Light LTR303 measurements|c210/sensor/light|506|{"ID":"LTR303","mes":[ {"nm":"Lt","vl":190}]}|
|FXAS21002|Gyroscope FXAS21002 measurements|c210/sensor/gyroscope|507|{"ID":"FXAS21002","mes":[ {"nm":"Gx","vl":25.33333},{"nm":"Gy","vl":67.55333},{"nm":"Gz","vl":33.44333}]}|
|MAXM10|Position|c210/position/nmea|508|{"ID":"MAXM10","mes":[{"nm":"Px","vl":38.0499625},{"nm":"Py","vl":23.8088328}]}|

These topics should be created in Thingstream portal, before trying to send data via Cellular (they should be created automatically when the redemption code is used) 
