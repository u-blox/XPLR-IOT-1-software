# Description
This folder contains:
* 3rd party tools that might be needed
* The compiled image(s) of the Sensor Aggregation Use Case application
* A batch script to help in quickly updating your XPLR-IOT-1 device


##### 3rd party tools

- **newtmgr.exe:** This program is used to update the firmware in the device when a bootloader is used (only for Windows). See [Bootloader information](../compile_options/bootloader_inclusion) and [Programming the firmware using a J-Link debugger](../Readme.md)


# XPLR-IOT-1 bootloader update process

XPLR-IOT-1 is updated through the Apache utility `newtmgr`. A batch file is included for a simple double-click installer. Alternately, the update(s) can be applied manually at a command prompt. Both methods assume use of the Windows operating system.

## Batch file

1. Download the new application core and network core firmware images and the newtmgr utility from the main XPLR-IOT-1 main repository
    * If there are no updates to the network core, there will only be an application core file.
2. Start bootloader mode by pressing and holding button 1 while resetting or power-cycling XPLR IOT-1.
3. Determine the COM port number for NORA-B1 on Interface 0 of the USB-UART interface (e.g., COM 15). This example assumes the example COM port enumeration of interface 0 shown at USB connection.
4. Open a file browser and navigate to the folder containing the firmware images.
5. Double-click on the batch file `Update_XPLR-IOT-1.bat`
6. When prompted, enter the COM port number (e.g., COM15).
7. When the update completes, press any key to close the window.
8. Reset XPLR-IOT1 (reset button, power switch, or command line: `nrfjprog –-reset`)

## Manual updates

The application on NORA-B1 can be updated through the MCUboot bootloader over UART. Each new image is uploaded to the XPLR-IOT-1 QSPI flash prior to writing it to the NORA-B1 flash. 

1. Download the new application core and network core firmware images and the newtmgr utility from the main XPLR-IOT-1 main repository
    * If there are no updates to the network core, there will only be an application core file.
2. Start bootloader mode by pressing and holding button 1 while resetting or power-cycling XPLR IOT-1.
3. Determine the COM port number for NORA-B1 on Interface 0 of the USB-UART interface. This example assumes the example COM port enumeration of interface 0 shown at USB connection.
4. Open a Windows command prompt.
5. Navigate to the folder containing the firmware images.
6. Update the network core (optional, depending on the requirements of the update):\
    `newtmgr.exe –-conntype=serial –-connstring="COM8,baud=115200" image upload  net_core_update.bin`
    * Replace `net_core_update.bin` with the actual filename of the downloaded network core update. Replace `COM8` with the actual COM port on interface 0.
7. Update the application core:\
    `newtmgr.exe –-conntype=serial –-connstring="COM8,baud=115200" image upload  app_core_update.bin`
    * Replace `app_core_update.bin` with the actual filename of the downloaded application core update. Replace `COM8` with the actual COM port on interface 0.
8. Reset XPLR-IOT1 (reset button, power switch, or command line: `nrfjprog –-reset`)

⚠	If updating the network core, both the network and application cores must be updated during the same bootloader session. If updating only the application core, the network core update can be bypassed.

The update sequence listed here assumes the availability of pre-configured binary files. Custom application code may also be updated through the bootloader.