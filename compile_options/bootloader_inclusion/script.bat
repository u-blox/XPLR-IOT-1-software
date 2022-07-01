:: Script to prepare the project to build the
:: firmware and the serial bootloader itself

echo off
ECHO "Copies files from this folder to src folder"

echo on
copy prj.conf ..\..\prj.conf
copy nrf5340dk_nrf5340_cpuapp.overlay ..\..\nrf5340dk_nrf5340_cpuapp.overlay
copy pm_static.yml ..\..\pm_static.yml

Xcopy /E /I boards ..\..\boards
Xcopy /E /I child_image ..\..\child_image

pause