"""
This tool abstracts away many of the Uniflash command line operations.
"""
import sys
import subprocess as sp
import argparse as ap

DSLITE        = r"dslite.bat"
UNIFLASH_PATH = r"C:/ti/uniflash_9.2.0/"

DUMMY_IMAGE_PATH = r"images/dummy.hex"
HSM_FW_PATH      = r"images/cc27xxx10_hsm_fw_v3.0.0.bin"
CCXML_FILE       = r"images/config.ccxml"
CONFIG           = f"--config=\"{CCXML_FILE}\" "

UNIFLASH              = UNIFLASH_PATH + DSLITE + " "
UNIFLASH_NO_MODE      = UNIFLASH + CONFIG
UNIFLASH_FLASH_MODE   = UNIFLASH + "--mode flash " + CONFIG
UNIFLASH_NO_CONN_MODE = UNIFLASH + "--mode noConnectFlash " + CONFIG

XDSDFU = UNIFLASH_PATH + r"deskdb/content/TICloudAgent/win/ccs_base/common/uscif/xds110/xdsdfu.exe -e"

def setup_arguments():
    """
    Sets up arguments for the uniscript command line tool.

    Returns:
        Namespace: The arguments passed to the script.
    """
    parser = ap.ArgumentParser(description="uniscript - Uniflash cmdline tool. Only supports CC2745R10 and CC2340R5 devices.")

    parser.add_argument("-d", "--device", required=True, help="Indicate device. Must pass device: cc27xx or cc23xx.")
    parser.add_argument("-s", "--serial", help="This will execute on a device with this serial number.")
    parser.add_argument("-m", "--multiple-devices", help="All connected devices will be executed.", action="store_true")
    parser.add_argument("-c", "--connected-devices", help="Will print a list of connected devices. Will exit after printing.", action="store_true")
    parser.add_argument("-a", "--app-hex", nargs="*", help="Path to application hex files.")
    parser.add_argument("-b", "--app-bin", nargs="*", help="Path to binary files. Must be in format: \"<app>.bin, <address>\"")
    parser.add_argument("-e", "--erase", help="Mass erases the chip including the HSM firmware.", action="store_true")
    parser.add_argument("-l", "--load-hsm", help="Will load the HSM firmware.", action="store_true")
    parser.add_argument("-v", "--hsm-version", help="Prints the HSM version.", action="store_true")
    parser.add_argument("-u", "--uniflash", help="Path to Uniflash", default=UNIFLASH_PATH)

    args = parser.parse_args()
    return args


def stdout_format(sp_output):
    """
    Formats the output of a subprocess.

    Args:
        sp_output (subprocess.CompletedProcess): The output of a subprocess.

    Returns:
        None

    Description:
        This function takes the output of a subprocess and formats it. It decodes the output and splits it into lines.
        Then, it checks if the line contains the string "info" and prints it with a tab at the beginning.

        The function assumes that the output of the subprocess is encoded in UTF-8.
    """
    decoded_output = sp_output.stdout.decode('utf-8').splitlines()
    for line in decoded_output:
        if "info" in line.split(":"):
            print(f"\t{line}")


def load_app(image_path, no_connect_flash=False, no_flash_erase=False):
    """
    Load an application onto the device.

    Args:
        image_path (str): The path to the application image.
        no_connect_flash (bool, optional): Whether to load the application without connecting the device. Defaults to False.
        no_flash_erase (bool, optional): Whether to skip flash erase. Defaults to False.

    Returns:
        None

    Raises:
        Exception: If there is an error during the process.

    Description:
        This function loads an application onto the device. It takes the path to the application image as the first argument.
        If the `no_connect_flash` parameter is set to True, it means that the chip is blank. In this case, the function
        sets the `cmd` variable to `UNIFLASH_NO_CONN_MODE`. Otherwise, it sets `cmd` to `UNIFLASH_FLASH_MODE`.
    """
    print(f"uniscript: Flashing {image_path}... ")
    if no_connect_flash:
        # This needs to be done if the chip is blank.
        cmd = UNIFLASH_NO_CONN_MODE
    else:
        # This needs to be done in the case that the chip isn't blank (after flashing the HSM).
        cmd = UNIFLASH_FLASH_MODE

    if no_flash_erase:
        cmd += "-l images/settings.ufsettings "

    cmd += f"-f \"{image_path}\""

    try:
        sp.run(cmd, check=True, stdout=sp.PIPE)
    except Exception:
        sys.exit()


def load_bin(bin_files, no_connect_flash=False, no_flash_erase=False):
    """
    Load binary files onto the device.

    Args:
        bin_files (list): A list of binary files to load.
        no_connect_flash (bool, optional): Whether to load the binary files without connecting the device. Defaults to False.
        no_flash_erase (bool, optional): Whether to skip flash erase. Defaults to False.

    Returns:
        None

    Raises:
        Exception: If there is an error during the process.

    Description:
        This function loads binary files onto the device. It takes a list of binary files as the first argument.
        If the `no_connect_flash` parameter is set to True, it means that the chip is blank. In this case, the function
        sets the `cmd` variable to `UNIFLASH_NO_CONN_MODE`. Otherwise, it sets `cmd` to `UNIFLASH_FLASH_MODE`.

        If the `no_flash_erase` parameter is set to True, the function appends `-l images/settings.ufsettings` to the `cmd`
        variable.

        The function then iterates over each binary file in the `bin_files` list. It splits the file and location using
        the comma delimiter. If the file or location is None, it prints an error message and exits the program.

        The function then constructs the command to load the binary file by appending the file and location to the `cmd`
        variable. It runs the command using the `sp.run()` function. If an exception is raised, the function calls
        `sys.exit()` to terminate the program.
    """
    if no_connect_flash:
        cmd = UNIFLASH_NO_CONN_MODE
    else:
        cmd = UNIFLASH_FLASH_MODE

    if no_flash_erase:
        cmd += "-l images/settings.ufsettings "

    for bin_file in bin_files:
        file, location = bin_file.split(",")
        if file is None or location is None:
            print("uniscript: No file or location provided.")
            sys.exit(-1)

        print(f"uniscript: Flashing {file} at {location}... ")
        temp_cmd = cmd + f"-f \"{file},{location}\""
        try:
            sp.run(temp_cmd, check=True, stdout=sp.PIPE)
        except Exception:
            sys.exit(-1)


def load_hsm(blank, hsm_version, device):
    """
    Load the HSM firmware onto the device.

    Args:
        blank (bool): Whether the device is blank.
        hsm_version (bool): Whether to print the HSM version.
        device (str): The device type.

    Returns:
        None

    Raises:
        Exception: If the device is a CC23xx device.
        Exception: If flashing the HSM firmware fails.
        Exception: If reading the HSM version fails.

    Description:
        This function loads the HSM firmware onto the device. It takes the `blank` parameter to determine
        whether the device is blank. If the `hsm_version` parameter is True, it prints the HSM version.
        If the `device` is "cc23xx", it raises an Exception.
    """
    if device == "cc23xx":
        print("uniscript: CC23xx devices do not support HSM.")
        sys.exit(-1)

    # load the dummy CCFG/SCFG image first.
    load_app(DUMMY_IMAGE_PATH, blank)

    # load the HSM firmware.
    print("uniscript: Flashing HSM firmware... ")

    cmd = UNIFLASH_NO_CONN_MODE + f"-s HsmImagePath=\"{HSM_FW_PATH}\" -a ProgramHsmImage -n 0"
    try:
        sp.run(cmd, check=True, stdout=sp.PIPE)
    except Exception:
        print("uniscript: Flashing HSM failed. Most likely due to ROM panic.")
        print("uniscript: Run 'uniscript -e' and retry.")
        sys.exit(-1)

    # read out the version.
    if hsm_version:
        print("uniscript: Reading HSM version... ", end="")

        cmd = UNIFLASH_NO_CONN_MODE + "-a ReadHsmSysInfo -n 0"
        try:
            hsm_version = sp.run(cmd, check=True, stdout=sp.PIPE)
        except Exception:
            sys.exit(-1)

        stdout_format(hsm_version)


def chip_erase(device):
    """
    Erase the chip.

    Args:
        device (str): The device type.

    Returns:
        None

    Raises:
        Exception: If the device is a CC23xx device.
        Exception: If erasing the chip fails.

    Description:
        This function erases the chip. It takes the `device` parameter to determine
        the device type.

        If the `device` is "cc27xx", it performs the chip erase twice. If the erase
        fails, it prints an error message and exits the program.

        If the `device` is "cc23xx", it performs the chip erase once. If the erase
        fails, it prints an error message and exits the program.

    """
    print("uniscript: Erasing chip... ")

    cmd = UNIFLASH_NO_CONN_MODE + "-a ChipEraseRetain"
    if device == "cc27xx":
        # For the CC27xx, this has to happen twice, otherwise the HSM will still
        # be on the chip.
        try:
            result = sp.run(cmd, check=True, stdout=sp.PIPE)
            result = sp.run(cmd, check=True, stdout=sp.PIPE)
        except Exception:
            print("uniscript: Erasing failed. Most likely due to ROM panic.")
            print("uniscript: Re-attempting erase...")
            result = sp.run(cmd, check=True, stdout=sp.PIPE)
            result = sp.run(cmd, check=True, stdout=sp.PIPE)
    elif device == "cc23xx":
        # Since there is no HSM, we can get away with erasing this chip one
        # time.
        try:
            result = sp.run(cmd, check=True, stdout=sp.PIPE)
        except Exception:
            print("uniscript: Erasing failed. Most likely due to ROM panic.")
            print("uniscript: Re-attempting erase...")
            result = sp.run(cmd, check=True, stdout=sp.PIPE)


def set_config(device, serial):
    """
    Set the configuration for a device.

    Parameters:
    - device (str): The type of device.
    - serial (str): The serial number of the device.

    Returns:
    - None

    Raises:
    - SystemExit: If the device is not supported.
    """
    serial_number = serial

    if device == "cc27xx" or device == "cc23xx":
        with open(CCXML_FILE, "r", encoding="utf-8") as f:
            lines = f.readlines()
            lines[21] = f"""<property id="-- Enter the serial number" Type="stringfield" Value="{serial_number}"/>\n"""
            if device == "cc23xx":
                lines[6]  = """<instance XML_version="1.2" href="drivers/tixds510cortexM0.xml" id="drivers" xml="tixds510cortexM0.xml" xmlpath="drivers"/>\n"""
                lines[8]  = """<property Type="choicelist" Value="1" id="Power Selection">\n"""
                lines[9]  = """<choice Name="Probe supplied power" value="1">\n"""
                lines[10] = """<property Type="stringfield" Value="3.3" id="Voltage Level"/>\n"""
                lines[13] = """<property Type="choicelist" Value="0" id="JTAG Signal Isolation"/>\n"""
                lines[25] = """<instance XML_version="1.2" desc="CC2340R5" href="devices/cc2340r5.xml" id="CC2340R5" xml="cc2340r5.xml" xmlpath="devices"/>\n"""
            elif device == "cc27xx":
                lines[6]  = """<instance XML_version="1.2" href="drivers/tixds510cortexM33.xml" id="drivers" xml="tixds510cortexM33.xml" xmlpath="drivers"/>\n"""
                lines[8]  = """<property Type="choicelist" Value="0" id="Power Selection">\n"""
                lines[9]  = """<choice Name="Target supplied power" value="0">\n"""
                lines[10] = """<property Type="choicelist" Value="0" id="Voltage Selection"/>\n"""
                lines[13] = """<property Type="choicelist" Value="1" id="JTAG Signal Isolation"/>\n"""
                lines[25] = """<instance XML_version="1.2" desc="CC2745R10" href="devices/cc2745r10.xml" id="CC2745R10" xml="cc2745r10.xml" xmlpath="devices"/>\n"""
            with open(CCXML_FILE, "w", encoding="utf-8") as f:
                f.writelines(lines)
    else:
        print(f"uniscript: Device {device} not supported.")
        sys.exit(-1)


def check_blank():
    """
    This function checks if the device is blank.

    Returns:
        bool: True if the device is blank, False otherwise.
    """
    print("uniscript: Checking if device is blank... ")
    cmd = UNIFLASH_NO_CONN_MODE + "-a BlankCheck"

    try:
        result = sp.run(cmd, check=True, stdout=sp.PIPE)
    except Exception:
        sys.exit(-1)

    lines = result.stdout.decode('utf-8')

    if lines.split()[-1] != "programmed":
        print("uniscript: Device is blank.")
        return True

    print("uniscript: Device is not blank.")
    return False


def load_mult_app(hex_files, blank):
    """
    Load multiple application files.

    Args:
        hex_files (list): A list of hex files to load.
        blank (bool): Indicates if the device is blank.

    Returns:
        None

    This function loads multiple application files. If there is only one file to load,
    it immediately flashes the device and returns. Otherwise, the first file is flashed
    with erasing, and the device is no longer considered blank. The rest of the files
    are loaded without erasing.
    """
    # If there is only one file to load, then immediately flash and return.
    if len(hex_files) == 1:
        load_app(str(hex_files[0]), blank, no_flash_erase=False)
        return

    # the first file will always flash with erasing.
    load_app(str(hex_files[0]), blank, no_flash_erase=False)

    # Since we've flashed the first program, it will no longer be blank.
    blank = False

    # load the rest of the files without erasing.
    hex_files = hex_files[1:]
    for file in hex_files:
        load_app(str(file), blank, no_flash_erase=True)


def get_serials():
    """
    Retrieves the serial numbers of connected devices.

    Returns:
        list: A list of serial numbers.

    This function runs the XDSDFU command to retrieve the connected devices. It then
    extracts the serial numbers from the output and returns them as a list. If there
    are no devices connected, it prints a message and exits the program.
    """
    serial_numbers = []
    result = sp.run(XDSDFU, check=True, stdout=sp.PIPE)
    lines = [x for x in result.stdout.decode('utf-8').split('\r\n') if x != '']
    if len(lines) <= 4:
        print("uniscript: No devices connected.")
        sys.exit(-1)
    devices = lines[3:]
    for i in range(5, len(devices), 8):
        serial_numbers.append(devices[i].split()[-1])
    return serial_numbers


def main():
    """
    The main function of the script.

    This function is the entry point of the script. It sets up the arguments, checks if the device is supported,
    detects and sets the serial, erases the chip if necessary, loads the HSM firmware if specified, loads multiple
    application files if specified, and prints a completion message.

    Parameters:
        None

    Returns:
        None
    """
    # Get the arguments. Although these will already be set, for debugging
    # purposes we will allow access to main.
    args = setup_arguments()

    if args.connected_devices:
        connected_devices = get_serials()
        print("uniscript: Connected devices:")
        for i, device in enumerate(connected_devices):
            print(f"\t{i}. {device}")
        sys.exit(0)
    
    if args.serial is not None and args.multiple_devices is True:
        print("uniscript: Serial number cannot be used with multiple devices.")
        sys.exit(-1)
    elif args.serial is not None:
        serial_numbers = [args.serial]
    elif args.multiple_devices:
        serial_numbers = get_serials()
    else:
        serial_numbers = [get_serials()[0]]

    if args.device != "cc27xx" and args.device != "cc23xx":
        print("uniscript: Device not supported.")
        sys.exit(-1)

    index = 0
    while True:
        blank = False

        print(f"uniscript: Setting serial number {serial_numbers[index]}... ")
        set_config(args.device, serial_numbers[index])

        if args.erase:
            chip_erase(args.device)
            blank = True
        else:
            # If the device is blank, then the mode for the debugger is different.
            blank = check_blank()

        if args.load_hsm:
            load_hsm(blank, args.hsm_version, args.device)
            # Since we've loaded the HSM, the device is no longer blank.
            # So if we tried to flash the app files after this, it would break.
            blank = False

        if args.app_hex:
            load_mult_app(args.app_hex, blank)
            blank = False

        if args.app_bin:
            # If we have flashed hex files, then we know we need to NOT erase before
            # flash.
            no_flash_erase = bool(args.app_hex)
            load_bin(args.app_bin, blank, no_flash_erase)
        
        if args.serial is None and args.multiple_devices is True:
            if index < (len(serial_numbers) - 1):
                index += 1
            else:
                break
        else:
            break

    print("uniscript: Done.")


if __name__ == '__main__':
    main()
