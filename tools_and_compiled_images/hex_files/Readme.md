## Compiled hex files

In this folder compiled images of the project are provided
* 'SensorAggregation_v0.3_MergedCores.hex' contains the application alone.
* 'SensorAggregation_v0.3_Bootloader_MergedCores.hex' contains the application and the bootloader.

These images contain the merged images for both Application and Network Cores of NORA-B1.

These images can be programmed in the device using a J-Link and [Nordic’s nRF Connect for Desktop](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-desktop) and more specifically the *Programmer app*. The device should be reset after programming.

![nrf connect programmer should be here.](/readme_images/nrfConnect_programmer.png "nrf connect programmer")


**Note**: These files cannot be used to program the device with the nrfjprog commands listed in [Programming the firmware using a J-Link debugger](../../Readme.md) section.



