:: Script to prepare the project to build the
:: firmware as an image that can be used with serial
:: bootloader to update the firmware in the device

echo off
ECHO "Copies files from this folder to src folder and deletes "boards" and "child_image" folders if they exist in src"

echo on
copy prj.conf ..\..\prj.conf
copy nrf5340dk_nrf5340_cpuapp.overlay ..\..\nrf5340dk_nrf5340_cpuapp.overlay
copy pm_static.yml ..\..\pm_static.yml

rmdir /s /q ..\..\boards
rmdir /s /q ..\..\child_image


pause