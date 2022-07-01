# Compile options description
This folder contains three sub-folders with the necessary files and scripts to modify the project for three build scenarios. The scripts only work in Windows, however they perform simple copy/overwrite/delete file functions, so it is easy to modify the project in other operating systems as well. Bash scripts which perform the same operations will be added soon. More information is given within each folder:
1.	Build the project as a simple image [(simple hex)](./simple_hex). This is the simplest form of build and contains nothing but the application firmware alone. This image can be used to program the device directly, using a J-Link debugger. It does not contain a bootloader. It cannot be used with a bootloader to update the device.

1.	Build an [updateable image](./updateable_image). This image can be used to update the firmware of the device using the Serial Bootloader. If programmed alone to a device without the bootloader it won’t be able to start.

1.	Build an image that contains both the bootloader and the Sensor Aggregation application firmware [(bootloader inclusion)](./bootloader_inclusion). This image can be programmed in the device using a JLink debugger. After that the device can be updated using the Serial Bootloader with updateable images.

XPLR-IOT-1 comes with a Serial Bootloader pre-programmed, so **if you do not have a J-Link**, please choose option 2, and update the device using the Serial Bootloader. Instruction on using the bootloader can be found [here](../Readme.md) and more detailed instructions can be found [here](./bootloader_inclusion).

**If you have a J-Link** you can use option 1 to program the application firmware, or option 2 if you want to include a Serial Bootloader along with the application.