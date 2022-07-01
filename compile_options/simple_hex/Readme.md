# Simple Image Build
This folder contains the prj.conf and overlay file for XPLR-IOT-1 to setup the project to build a simple image for Sensor Aggregation application. 

The prj.conf file does not contain any bootloader definitions/options.
The overlay file does not contain any definitions for partitions used by the bootloader.

The script file in this folder:
- Overwrites the prj.conf and overlay files in the project's main folder with the ones in this folderr.
- Deletes the "pm_static.yml" file (if exists) from the project's main folder.
- Deletes the "boards" and "child_image" folders from the project's main folder.

Just run the script and build the project.

This allows the build of a simple application image. After you build the project you can navigate to your build folder, and find the *build/zephyr/zephyr.hex* file created. This is the hex file you can use to program your device with the application.
