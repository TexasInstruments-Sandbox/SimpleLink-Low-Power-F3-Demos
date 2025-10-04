# uniscript
`uniscript` is a tool to allow developers to do typical processes that would be
done in Uniflash through the command line. It supports flashing multiple
application images (both binaries and hex files), the HSM firmware, and erasing 
the chip. It also supports multiple device operations!

**Note:** This script is only applicable to the CC2745R10 and CC2340R5 devices.
Support for other devices will be available in the future.

# Prerequisites
1. Uniflash version 9.2 must be installed.
  * If Uniflash is *not* installed in the default directory, pass the Uniflash
    install folder using the `-u` flag.
2. Python version 3.9 or higher must be used.
3. The script must be ran in the same directory as the `images` folder.

# Usage
```
$ python ./uniscript.py -h
usage: uniscript.py [-h] -d DEVICE [-s SERIAL] [-m] [-c] [-a [APP_HEX ...]] [-b [APP_BIN ...]]
                    [-e] [-l] [-v] [-u UNIFLASH]

uniscript - Uniflash cmdline tool. Only supports CC2745R10 and CC2340R5 devices.

options:
  -h, --help            show this help message and exit
  -d DEVICE, --device DEVICE
                        Indicate device. Must pass device: cc27xx or cc23xx.
  -s SERIAL, --serial SERIAL
                        This will execute on a device with this serial number.
  -m, --multiple-devices
                        All connected devices will be executed.
  -c, --connected-devices
                        Will print a list of connected devices. Will exit after printing.
  -a [APP_HEX ...], --app-hex [APP_HEX ...]
                        Path to application hex files.
  -b [APP_BIN ...], --app-bin [APP_BIN ...]
                        Path to binary files. Must be in format: "<app>.bin, <address>"
  -e, --erase           Mass erases the chip including the HSM firmware.
  -l, --load-hsm        Will load the HSM firmware.
  -v, --hsm-version     Prints the HSM version.
  -u UNIFLASH, --uniflash UNIFLASH
                        Path to Uniflash
```
If `-s` or `-m` is not supplied, the first found device will be executed on.

An example usage for loading a Secondary Secure Bootloader plus two app images would be the following:

```$ python uniscript.py -d cc27xx -a SSB_sb.hex -b "primary.bin,0x7000" "secondary.bin,0x39000" -l```

1. Indicate what device this is for: `-d`
2. This will first load the HSM: `-l`
3. Then it will load `SSB_sb.hex`: `-a`
4. Then it will load `primary.bin` and `secondary.bin`: `-b`.

NOTE: .hex files will *always* be loaded before binary files.