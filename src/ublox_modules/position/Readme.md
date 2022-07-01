#	MAXM10S and position
MAXM10S is handled in Sensor Aggregation Firmware sometimes as a sensor and others as a module (as it is used with ubxlib).
So, it has the same functions as a sensor: enable/disable, publish, set period (see 3.2.1)
The extra commands in this module are:
-	Comm=nora
-	Comm=usb
-	Set timeout

##### Set timeout
Usually when sensors are asked for their values, they respond immediately. The MAXM10S may take some time since its power up to obtain a valid GNSS position. That is why the timeout parameter was added. 
This parameter defines the maximum waiting time for the module to respond with a valid position. If this timeout period expires the request is aborted and no position is obtained. The timeout period should not be greater than the update period. At each update period the MAX is asked again for its position with a new request.

##### Comm=Nora/Comm=usb
MAXM10S has a UART interface which can either be connected to NORA-B1 or the UART to usb adapter of the XPLR-IOT-1. The latter is used to connect MAXM10S directly to a host PC.

If the user wants to connect MAXM10S to u-center, he should power on the module and use the command comm=usb. Then he will be able to connect to u-center. However, during this time, MAXM10S will not be able to communicate with NORA-B1 so, no position can be obtained from the firmware.

Sending the comm=nora connects MAX to NORA again and the firmware can use the module again normally.


When the ubxlib is initialized/deinitialized for SARA-R5 and NINA-W156 (see below) it is also initialized/deinitialized for MAXM10S too, however this does not have a serious effect on its operation.
