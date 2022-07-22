@echo off
echo.
echo **********************************************
echo ** XPLR-IOT-1 bootloader update utility     **
echo **********************************************
echo ** Ensure XPLR-IOT-1 is in bootloader mode. **
echo ** Press and hold button 1 while resetting  **
echo ** or power-cycling the device.             **
echo **********************************************
echo.
set /p "COMPORT=Enter the COM port number of Interface 0 (e.g. COM15): "
echo.
echo Updating Network Core:
newtmgr.exe --conntype=serial --connstring="%COMPORT%,baud=115200" image upload SensorAggregation_v0.3_NetCore.bin
echo.
echo Updating Application Core:
newtmgr.exe --conntype=serial --connstring="%COMPORT%,baud=115200" image upload SensorAggregation_v0.3_AppCore.bin
echo.
pause