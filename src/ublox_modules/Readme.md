# Handling u-blox Modules

There are four u-blox modules in XPLR-IOT-1:
-	NORA-B1 (Which is the module that runs the firmware, what the user can program)
-	[SARA-R5](./cell) (handles cellular connections)
-	[NINA-W156](./wifi) (handles Wi-Fi connections)
-	[MAXM10S](./position) (GNSS positioning module)

These modules have some following functions common:
-	Power on
-	Power off

As the name suggests these functions power up/down the respective modules.
Also, all these modules are handled using ubxlib library in the code. That creates some sort of dependency between the status of these modules.