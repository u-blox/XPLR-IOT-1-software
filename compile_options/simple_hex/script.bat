:: Script to prepare the project to build the
:: firmware as a simple image that can be programmed
:: with JLink. This image cannot be used with serial
:: bootloader to update the firmware in the device

echo off
ECHO "Copies files from this folder to src folder and deletes "boards" and "child_image" folders and pm_static.yml if they exist in src"

echo on
copy prj.conf ..\..\prj.conf
copy nrf5340dk_nrf5340_cpuapp.overlay ..\..\nrf5340dk_nrf5340_cpuapp.overlay

del ..\..\pm_static.yml
rmdir /s /q ..\..\boards
rmdir /s /q ..\..\child_image


pause