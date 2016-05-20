# CAN-Bootloader
CCS OTP Flash project and utility program to allow team to reprogram F28035 microcontrollers without the use of JTAG.

## Repository Contents
### CAN-Bootloader-Utility
This utility is used to send the program to be bootloaded onto the destination microcontroller over CAN in the correct format. This utility relies on TI's hex converter utility to convert compiled .out files to ASCII encoded program files.

See TI's Boot Rom guide for more information: http://www.ti.com/lit/ug/sprugo0b/sprugo0b.pdf

hex2000 compile command: `hex2000.exe "your_file.out" -boot -gpio8 -a`

To use the utility, make sure to build the rust program for your target (See http://doc.crates.io/guide.html for details). There are multiple parameters that can be passed to the utility in order to change the bootloading process.

* -i: Input ASCII encoded program to bootload over CAN
* -d: Device CAN ID which should be bootloaded (Command ID for that device). This will cause the bootloader to send the special start bootload command message which will cause the device to reset, enter bootloading, and wait for the new program contents to be received)
* -b: Bypass mode. If the device is already in it's bootload state and waiting for program contents, this mode should be used to skip sending the bootload command message.

### F28035_Flash_CAN_OTP
A flash image for a F28035 to install the bootloader in the OTP section of memory for the device. 

___THIS IS IRREVERSIBLE ONCE FLASHED AND CANNOT BE UPGRADED AT THIS TIME___


