# BQ27520 Battery Gauge Driver
This folder contains a BQ27520 Battery Gauge basic driver, as a Zephyr module that can be used by the main application.

### Known Issues
Trying to use the `BQ27520_DESIGN_CAPACITY` and `BQ27520_TERMINATE_VOLTAGE` configuration options via the *prj.conf* in your application may trigger compilation errors.
Nevertheless, the battery gauge is set up to work properly with the battery used in XPLR-IOT-1 devices and these configuration options do not need to be altered in the *prj.conf* file for use with an XPLR-IOT-1 device.