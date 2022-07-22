# Updateable Image Build
This folder contains the **prj.conf** and **overlay** file for XPLR-IOT-1 to setup the project to build an updateable image for Sensor Aggregation application. It also contains a **pm_static.yml** file.

The **prj.conf** file apart from the application definitions it just contains a directive to compile an image with the useof the bootloader. The **pm_static.yml** partitioning file must be provided to make sure it will be in sync with the partitionning defined by the bootloader. The **overlay** file is needed by the hardware to know whehere to find the external flash and the emulation memory partition.

The script file in this folder:
- Overwrites the prj.conf, and overlay files in the project's main folder with the ones in this folder.
- Overwrites or copies the pm_static.yml in the project's main folder.
- Deletes the "boards" and "child_image" folders from the project's main folder.

Just run the script and build the project.

This allows the build of an updateable application image. After you build the project you can navigate to your build folder, and find the:
* **build/hci_rpmsg/zephyr/signed_by_b0_app.bin** file created. This file contains the updateable image for the Net Core of NORA-B1.
* **build/zephyr/app_update.bin** file created. This file contains the updateable image for the Application Core of NORA-B1.

In order to use them **FIRST update the Net Core and THEN the Application Core** using the newtmgr.exe tool in [tools_and_compiled_images](./tools_and_compiled_images/) folder (check the batch script file in that folder to see how you can update your device. The bin files in that folder are the same mentioned here, they are just renamed).
