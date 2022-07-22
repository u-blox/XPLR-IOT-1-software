# Image Build that contains the Bootloader
This folder contains the **prj.conf** and **overlay** file for XPLR-IOT-1 to setup the project to build an image for Sensor Aggregation application that also contains the bootloader. It also contains a **pm_static.yml** file and the *boards* and *child image* folders needed to compile the bootloader.

The **prj.conf** file apart from the application definitions it also contains directives to setup and compile the bootloader. The **pm_static.yml** partitioning file defines the partitions used by the bootloader. The **overlay** file is needed by the hardware to know whehere to find the external flash and the emulation memory partition.

The script file in this folder:
- Overwrites the prj.conf, and overlay files in the project's main folder with the ones in this folder.
- Overwrites or copies the pm_static.yml in the project's main folder.
- Copies the "boards" and "child_image" folders to the project's main folder.

Just run the script and build the project.

This allows the build of an application image that also contains the bootloader. After you build the project you can navigate to your build folder, and find the **build/zephyr/merged_domains.hex** file created. This is the hex file you can use to program your device with a J-Link programmer via the nRF Connect-> Programmer application.

**Note**: If you want to use nrfjprog commands, instead of the nRF Connect -> Programmer application, different files are needed from the build folder, since you need to program each core separately.

The compiled image from this build configuration can be found [here](./tools_and_compiled_images/hex_files/SensorAggregation_v0.3_Bootloader_MergedCores.hex)

## Bootloader Information

The bootloader contained in this build is slightly modified to allow the use of the XPLR-IOT-1 external flash memory. The original bootloader provided by the Nordic SDK uses a dual bank system to update the firmware on the device. This however, restricts the size of the memory that can be used for the application to half.

To work around this we use the external memory in this configuration and modify the default partitioning. This bootloader can be used to update both the net and app cores on NORA-B1 MCU (nRF5340)


###	Overview
The integrated bootloader is based on MCUBoot (used by Zephyr).
For your programming needs you can use 2 tools depending on your operating system:

1.	Newtgmr - for Windows, available [here](../../tools)
2.	Mcumgr – for Linux, available [here](https://docs.zephyrproject.org/3.0.0/guides/device_mgmt/mcumgr.html)
**NOTE for mcumgr** : Needs to built from sources

#####	Features
MCUboot offers the following function regarding firmware update:
- Can flash *.bin images using the serial interface for either/or:
-- The application core
-- The network core
It’s not a prerequisite to always update both image in one session
- No need to use a JTAG programmer


### Usage Instructions

##### Firmware images
In order to flash images we need 2 files generated during build:
- app_update.bin, belonging to the application core CPU
- net_core_app_update.bin, belonging to the net core CPU

These files are generated only if the MCUBootloader and Network stack (in the case of the net_core) directive are enabled in the build options of a Zephyr project.

Both Files can be found under:
C:\...\<project_name>\build\zephyr
**NOTE:** If there is NO network core functionality, i.e. Bluetooth then net_core_app_update.bin will not be generated

Now we can simply write the update bin images to XPLR-IOT-1.

##### Enabling the bootloader
To enter bootloader mode, XPLR-IOT-1 should be reset while button 1 is pressed. The button should be released about one second after the device is reset. Reset can be done either by pressing XPLR-IOT-1 reset button, or by turning the device off and on while pressing button 1.

After this procedure the device is ready to accept the appropriate commands to update its firmware.

#####	Updating the firmware
The following steps are recommended to make the procedure as easy as possible:

######	Windows
1.	Create a directory in a convenient location, (e.g., in Desktop) and name it as you please (e.g., “Firmware”)
2.	Copy newtmgr.exe (Windows) inside Firmware directory. (This step is not required if the location of newtmgr.exe is set to the environmental variables of Windows and the command line recognizes the newtmgr.exe command)
3.	Find serial COM port (e.g., COM18) for Interface 0, using Windows Device Manager
 
4.	Open a command line in the Firmware directory and upload the new firmware using the commands (where COM18 replace with the number of your setup):
```
newtmgr.exe --conntype=serial --connstring="COM18,baud=115200" image upload  net_core_app_update.bin
newtmgr.exe --conntype=serial --connstring="COM18,baud=115200" image upload  app_update.bin
nrfjprog –reset (or reset the device via on/off or reset button – without holding button 1 this time)
```
When net_core is not used you can omit the first command. 
However, when net_core is used, it **SHOULD** be updated before the app core.

######	Linux

1.	Create a directory in a convenient location, (e.g., in Desktop) and name it as you please (e.g., “Firmware”)
2.	Copy mcumgr (Windows) inside Firmware directory
3.	Find serial COM port (e.g., ttyUSB0  ) by executing the following command:
```
ls -al /dev/ | grep ttyUSB
```
4.	Use the following commands to upload the new firmware:
```
sudo chmod 666 /dev/ttyUSB0
newtmgr.exe --conntype=serial --connstring="/dev/ttyUSB0,baud=115200" image upload net_core_app_update.bin -n -e 3
b.	newtmgr.exe --conntype=serial --connstring="/dev/ttyUSB0,baud=115200" image upload app_update.bin -n -e 3
reset the device
```
 “ttyUSB0”, depends on your setup
When net_core is not used you can omit step a. 
However, when net_core is used, it **SHOULD** be updated before the app core.

###	Warning and Caveats
When updating both the net and app cores, be sure to update the net core first; failing to do so might break the booting sequence. This will not break the bootloader itself.

Make sure to wait for around 2 minutes after we upload the net CPU core application in order for XPLR-IOT-1 to be able to write it in the proper flash partition.
