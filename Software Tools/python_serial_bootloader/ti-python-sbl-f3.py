#!/usr/bin/env python3

# Copyright (c) 2014, Jelmer Tiete <jelmer@tiete.be>.
# Copyright (c) 2026, Texas Instruments Incorporated
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote
#    products derived from this software without specific prior
#    written permission.

# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Implementation based on stm32loader by Ivan A-R <ivan@tuxotronic.org>

# Serial boot loader over UART for CC13xx / CC2538 / CC26xx
# Based on the info found in TI's swru333a.pdf (spma029.pdf)
#
# Bootloader only starts if no valid image is found or if boot loader
# backdoor is enabled.
# Make sure you don't lock yourself out!! (enable backdoor in your firmware)
# More info at https://github.com/JelmerT/cc2538-bsl

from subprocess import Popen, PIPE
from enum import Enum

import sys
import getopt
import glob
import math
import time
import os
import struct
import binascii
import traceback

try:
    # Probe magic in a subprocess — a broken libmagic DLL causes a native
    # crash that Python's except cannot catch, so we isolate the risk.
    import subprocess as _sp, sys as _sys
    _result = _sp.run(
        [_sys.executable, '-c', 'import magic; magic.from_file'],
        capture_output=True, timeout=5)
    if _result.returncode == 0:
        import magic
        have_magic = True
    else:
        have_magic = False
    del _sp, _sys, _result
except Exception:
    have_magic = False

try:
    from intelhex import IntelHex
    have_hex_support = True
except ImportError:
    have_hex_support = False

# version
__version__ = "1.0"

# Verbose level
QUIET = 100

# Device Type
DEVICE_TYPE = ''

SUPPORTED_DEVICES = {
                     'cc2340r5': 'CC23XX',
                     'cc2745r10q1': 'CC27XXX10',
                     }

try:
    import serial
except ImportError:
    print('{} requires the Python serial library'.format(sys.argv[0]))
    print('Please install it with:')
    print('')
    print('   pip3 install pyserial')
    sys.exit(1)


def mdebug(level, message, attr='\n'):
    if QUIET >= level:
        print(message, end=attr, file=sys.stderr)

# Takes chip IDs (obtained via Get ID command) to human-readable names
CHIP_ID_STRS = {0xb964: 'CC2538',
                0xb965: 'CC2538',
                0x2dda: 'CC2340',
                0x1a96: 'CC2340',  # CC2340R53
                0xaf23: 'CC2745R10Q1',
                }

class CmdException(Exception):
    pass


class FirmwareFile(object):
    HEX_FILE_EXTENSIONS = ('hex', 'ihx', 'ihex')

    def __init__(self, path):
        """
        Read a firmware file and store its data ready for device programming.

        This class will try to guess the file type if python-magic is available.

        If python-magic indicates a plain text file, and if IntelHex is
        available, then the file will be treated as one of Intel HEX format.

        In all other cases, the file will be treated as a raw binary file.

        In both cases, the file's contents are stored in bytes for subsequent
        usage to program a device or to perform a crc check.

        Parameters:
            path -- A str with the path to the firmware file.

        Attributes:
            bytes: A bytearray with firmware contents ready to send to the
            device
        """
        self._crc32 = None
        firmware_is_hex = False

        if have_magic:
            file_type = magic.from_file(path, mime=True)

            if file_type == 'text/plain':
                firmware_is_hex = True
                mdebug(5, "Firmware file: Intel Hex")
            elif file_type == 'application/octet-stream':
                mdebug(5, "Firmware file: Raw Binary")
            else:
                error_str = "Could not determine firmware type. Magic " \
                            "indicates '%s'" % (file_type)
                raise CmdException(error_str)
        else:
            if os.path.splitext(path)[1][1:] in self.HEX_FILE_EXTENSIONS:
                firmware_is_hex = True
                mdebug(5, "Your firmware looks like an Intel Hex file")
            else:
                mdebug(5, "Cannot auto-detect firmware filetype: Assuming .bin")

            mdebug(10, "For more solid firmware type auto-detection, install "
                       "python-magic.")
            mdebug(10, "Please see the readme for more details.")

        if firmware_is_hex:
            if have_hex_support:
                ih = IntelHex(path)
                # Store each contiguous segment as (start_addr, bytearray).
                # tobinarray() without bounds fills the gap between the lowest
                # and highest address (e.g. app at 0x0 and CCFG at 0x4E020000)
                # producing a ~1.25 GB blob that breaks address tracking and
                # wastes time. Segments() gives us only the real data ranges.
                self.segments = [
                    (start, bytearray(ih.tobinarray(start=start, end=end - 1)))
                    for start, end in ih.segments()
                ]
                # For backward compatibility expose .bytes as the first segment
                self.bytes = self.segments[0][1] if self.segments else bytearray()
                mdebug(5, "Hex file contains %d segment(s):" % len(self.segments))
                for addr, data in self.segments:
                    mdebug(5, "  0x%08X  %d bytes" % (addr, len(data)))
                return
            else:
                error_str = "Firmware is Intel Hex, but the IntelHex library " \
                            "could not be imported.\n" \
                            "Install IntelHex in site-packages or program " \
                            "your device with a raw binary (.bin) file.\n" \
                            "Please see the readme for more details."
                raise CmdException(error_str)

        with open(path, 'rb') as f:
            self.bytes = bytearray(f.read())
        self.segments = [(0, self.bytes)]

    def crc32(self):
        """
        Return the crc32 checksum of the firmware image

        Return:
            The firmware's CRC32, ready for comparison with the CRC
            returned by the ROM bootloader's COMMAND_CRC32
        """
        if self._crc32 == None:
            self._crc32 = binascii.crc32(bytearray(self.bytes)) & 0xffffffff

        return self._crc32


class CommandInterfaceF3(object):
    class SBL_CODE:
        CMD_PING = 0x20
        CMD_GET_STATUS = 0x21
        CMD_GET_PART_ID = 0x22
        CMD_RESET = 0x23
        CMD_CHIP_ERASE = 0x24
        CMD_CRC32 = 0x25
        CMD_DOWNLOAD = 0x26
        CMD_DOWNLOAD_CRC = 0x27
        CMD_SEND_DATA = 0x28

        RET_SUCCESS = 0x40
        RET_UNKNOWN_CMD = 0x41
        RET_INVALID_CMD = 0x42
        RET_INVALID_ADR = 0x43
        RET_FLASH_FAIL = 0x44
        RET_CRC_FAIL = 0x45

        ACK_BYTE = 0xCC
        NACK_BYTE = 0x33
        SYNC_BYTE = 0x55
            

    def open(self, aport=None, abaudrate=500000):
        # Try to create the object using serial_for_url(), or fall back to the
        # old serial.Serial() where serial_for_url() is not supported.
        # serial_for_url() is a factory class and will return a different
        # object based on the URL. For example serial_for_url("/dev/tty.<xyz>")
        # will return a serialposix.Serial object for Ubuntu or Mac OS;
        # serial_for_url("COMx") will return a serialwin32.Serial oject for Windows OS.
        # For that reason, we need to make sure the port doesn't get opened at
        # this stage: We need to set its attributes up depending on what object
        # we get.
        try:
            self.sp = serial.serial_for_url(aport, do_not_open=True, timeout=10, write_timeout=10)
        except AttributeError:
            self.sp = serial.Serial(port=None, timeout=10, write_timeout=10)
            self.sp.port = aport

        if ((os.name == 'nt' and isinstance(self.sp, serial.serialwin32.Serial)) or \
           (os.name == 'posix' and isinstance(self.sp, serial.serialposix.Serial))):
            self.sp.baudrate=abaudrate        # baudrate
            self.sp.bytesize=8                # number of databits
            self.sp.parity=serial.PARITY_NONE # parity
            self.sp.stopbits=1                # stop bits
            self.sp.xonxoff=0                 # s/w (XON/XOFF) flow control
            self.sp.rtscts=0                  # h/w (RTS/CTS) flow control
            self.sp.timeout=0.5               # set the timeout value

        self.sp.open()

    def close(self):
        self.sp.close()

    def _wait_for_ack(self, info="", timeout=1):
        stop = time.time() + timeout
        got = bytearray(2)
        while got[-2] != 00 or got[-1] not in (self.__class__.SBL_CODE.ACK_BYTE,
                                               self.__class__.SBL_CODE.NACK_BYTE):
            got += self._read(1)
            if time.time() > stop:
                raise CmdException("Timeout waiting for ACK/NACK after '%s'"
                                   % (info,))

        # Our bytearray's length is: 2 initial bytes + 2 bytes for the ACK/NACK
        # plus a possible N-4 additional (buffered) bytes
        mdebug(10, "Got %d additional bytes before ACK/NACK" % (len(got) - 4,))

        # wait for ask
        ask = got[-1]

        if ask == self.__class__.SBL_CODE.ACK_BYTE:
            # ACK
            return 1
        elif ask == self.__class__.SBL_CODE.NACK_BYTE:
            # NACK
            mdebug(10, "Target replied with a NACK during %s" % info)
            return 0

        # Unknown response
        mdebug(10, "Unrecognised response 0x%x to %s" % (ask, info))
        return 0

    def _encode_addr(self, addr):
        byte3 = (addr >> 0) & 0xFF
        byte2 = (addr >> 8) & 0xFF
        byte1 = (addr >> 16) & 0xFF
        byte0 = (addr >> 24) & 0xFF
        return bytes([byte0, byte1, byte2, byte3])

    def _decode_addr(self, byte0, byte1, byte2, byte3):
        return ((byte3 << 24) | (byte2 << 16) | (byte1 << 8) | (byte0 << 0))

    def _calc_checks(self, cmd, addr, size):
        return ((sum(bytearray(self._encode_addr(addr))) +
                 sum(bytearray(self._encode_addr(size))) +
                 cmd) & 0xFF)

    def _write(self, data, is_retry=False):
        if type(data) == int:
            assert data < 256
            goal = 1
            written = self.sp.write(bytes([data]))
        elif type(data) == bytes or type(data) == bytearray:
            goal = len(data)
            written = self.sp.write(data)
        else:
            raise CmdException("Internal Error. Bad data type: {}"
                               .format(type(data)))

        if written < goal:
            mdebug(10, "*** Only wrote {} of target {} bytes"
                   .format(written, goal))
            if is_retry and written == 0:
                raise CmdException("Failed to write data on the serial bus")
            mdebug(10, "*** Retrying write for remainder")
            if type(data) == int:
                return self._write(data, is_retry=True)
            else:
                return self._write(data[written:], is_retry=True)

    def _read(self, length):
        return bytearray(self.sp.read(length))

    def sendAck(self):
        self._write(0x00)
        self._write(self.__class__.SBL_CODE.ACK_BYTE)
        return

    def sendNAck(self):
        self._write(0x00)
        self._write(self.__class__.SBL_CODE.NACK_BYTE)
        return

    def receivePacket(self):
        got = self._read(2)

        size = got[0]  # rcv size
        chks = got[1]  # rcv checksum
        data = bytearray(self._read(size - 2))  # rcv data

        mdebug(10, "*** received %x bytes" % size)
        if chks == sum(data) & 0xFF:
            self.sendAck()
            return data
        else:
            self.sendNAck()
            # TODO: retry receiving!
            raise CmdException("Received packet checksum error")
            return 0

    def sendSynch(self):
        cmd = self.__class__.SBL_CODE.SYNC_BYTE

        # flush serial input buffer for first ACK reception
        self.sp.flushInput()

        mdebug(10, "*** sending synch sequence")
        self._write(cmd)  # send U
        self._write(cmd)  # send U
        return self._wait_for_ack("sendSynch", 2)

    def checkLastCmd(self):
        stat = self.cmdGetStatus()
        if not (stat):
            raise CmdException("No response from target on status request. "
                               "(Did you disable the bootloader?)")

        if stat[0] == self.__class__.SBL_CODE.RET_SUCCESS:
            mdebug(10, "Command Successful")
            return 1
        else:
            mdebug(0, "Target returned: 0x%x" % (stat[0]))
            return 0

    def cmdPing(self):
        # BLDR_CMD_PING
        cmd = self.__class__.SBL_CODE.CMD_PING
        lng = 3

        self._write(lng)  # send size
        self._write(cmd)  # send checksum
        self._write(cmd)  # send data

        mdebug(10, "*** Ping command (0x%x)" % (cmd))
        if self._wait_for_ack("Ping (0x%x)" % (cmd)):
            return self.checkLastCmd()

    def cmdReset(self):
        # BLDR_CMD_RESET
        cmd = self.__class__.SBL_CODE.CMD_RESET
        lng = 3

        self._write(lng)  # send size
        self._write(cmd)  # send checksum
        self._write(cmd)  # send data

        mdebug(10, "*** Reset command (0x%x)" % (cmd))
        if self._wait_for_ack("Reset (0x%x)" % (cmd)):
            return 1

    def cmdGetChipId(self):
        # BLDR_CMD_GET_PART_ID
        # should return 4 bytes
        cmd = self.__class__.SBL_CODE.CMD_GET_PART_ID
        lng = 3

        self._write(lng)  # send size
        self._write(cmd)  # send checksum
        self._write(cmd)  # send data

        mdebug(10, "*** GetChipId command (0x%x)" % (cmd))
        if self._wait_for_ack("Get ChipID (0x%x)" % (cmd)):
            # 4 byte answer
            version = self.receivePacket()
            if self.checkLastCmd():
                assert len(version) == 4, ("Unreasonable chip "
                                           "id: %s" % repr(version))
                mdebug(10, "    Version 0x%02X%02X%02X%02X" % tuple(version))
                
                # TODO: Toby: consider returning all 4 bytes
                chip_id = (version[2] << 8) | version[3]
                
                return chip_id
            else:
                raise CmdException("GetChipID (0x%x) failed" % (cmd))

    def cmdGetStatus(self):
        # BLDR_CMD_GET_STATUS
        cmd = self.__class__.SBL_CODE.CMD_GET_STATUS
        lng = 3 # no change

        self._write(lng)  # send size
        self._write(cmd)  # send checksum
        self._write(cmd)  # send data

        mdebug(10, "*** GetStatus command (0x%x)" % (cmd))
        if self._wait_for_ack("Get Status (0x%x)" % (cmd)):
            stat = self.receivePacket()
            return stat

    def cmdBankErase(self):
        # BLDR_CMD_CHIP_ERASE
        # note: for FlashBldr, the device will not respond, since it will self erase
        cmd = self.__class__.SBL_CODE.CMD_CHIP_ERASE
        lng = 3

        self._write(lng)  # send length
        self._write(cmd)  # send checksum
        self._write(cmd)  # send cmd

        mdebug(10, "*** Bank Erase command(0x%x)" % (cmd))
        if self._wait_for_ack("Bank Erase (0x%x)" % (cmd), 10):
            return self.checkLastCmd()

    def cmdCRC32(self, addr, size, expectedCrc):
        # BLDR_CMD_CRC32
        cmd = self.__class__.SBL_CODE.CMD_CRC32
        lng = 11 # 3 bldr pkt bytes, +4 bytes for start addr, +4 bytes for sz
        lng += 4 # +4 bytes for expected CRC

        self._write(lng)  # send length
        self._write((self._calc_checks(cmd, addr, size) + expectedCrc) & 0xFF)  # send checksum
        self._write(cmd)  # send cmd
        self._write(self._encode_addr(addr))  # send addr
        self._write(self._encode_addr(size))  # send size
        self._write(self._encode_addr(expectedCrc))  # send expected CRC

        mdebug(10, "*** CRC32 command(0x%x)" % (cmd))
        if self._wait_for_ack("Get CRC32 (0x%x)" % (cmd), 1):
            crc = self.receivePacket()
            if self.checkLastCmd():
                return self._decode_addr(crc[3], crc[2], crc[1], crc[0])

    def cmdDownload(self, addr, size):
        cmd = self.__class__.SBL_CODE.CMD_DOWNLOAD
        lng = 11 # 3 bldr pkt bytes, +4 bytes for start addr, +4 bytes for sz

        # not sure if this is needed... since it should be valid to program individual bytes
        # can keep this in, because we are likely to use a 4-byte aligned size
        if (size % 4) != 0:  # check for invalid data lengths
            raise Exception('Invalid data size: %i. '
                            'Size must be a multiple of 4.' % size)

        self._write(lng)  # send length
        self._write(self._calc_checks(cmd, addr, size))  # send checksum
        self._write(cmd)  # send cmd
        self._write(self._encode_addr(addr))  # send addr
        self._write(self._encode_addr(size))  # send size

        mdebug(10, "*** Download command (0x%x)" % (cmd))
        if self._wait_for_ack("Download (0x%x)" % (cmd), 2):
            return self.checkLastCmd()

    def cmdSendData(self, data):
        # BLDR_CMD_SEND_DATA
        cmd = self.__class__.SBL_CODE.CMD_SEND_DATA
        lng = len(data)+3
        # TODO: check total size of data!! max 252 bytes!
        if (lng > 255):
            raise Exception('Invalid packet length: %i bytes. '
                            'Max size is 255 bytes.' % lng)

        self._write(lng)  # send size
        self._write((sum(bytearray(data))+cmd) & 0xFF)  # send checksum
        self._write(cmd)  # send cmd
        self._write(bytearray(data))  # send data

        mdebug(10, "*** Send Data (0x%x)" % (cmd))
        if self._wait_for_ack("Send data (0x%x)" % (cmd), 10):
            return self.checkLastCmd()


# Complex commands section

    def writeMemory(self, addr, data):
        # Bootloader requires size to be a multiple of 4; pad with 0xFF
        if len(data) % 4 != 0:
            data = data + bytearray(b'\xff' * (4 - len(data) % 4))
        total_bytes = len(data)
        lng = total_bytes
        # amount of data bytes transferred per packet (theory: max 252 + 3)
        trsf_size = 248
        empty_packet = bytearray((0xFF,) * trsf_size)

        # TODO: show warning in case CCFG would disable the ROM SBL.
        # it might make sense to do this when checking all the input files

        mdebug(5, "Writing %(lng)d bytes starting at address 0x%(addr)08X" %
               {'lng': lng, 'addr': addr})

        offs = 0
        addr_set = 0
        write_start = time.time()
        bytes_written = 0

        # check if amount of remaining data is less then packet size
        while lng > trsf_size:
            # skip packets filled with 0xFF
            if data[offs:offs+trsf_size] != empty_packet:
                if addr_set != 1:
                    # set starting address if not set
                    self.cmdDownload(addr, lng)
                    addr_set = 1

                self.cmdSendData(data[offs:offs+trsf_size])
                bytes_written += trsf_size

                elapsed = time.time() - write_start
                rate = bytes_written / elapsed if elapsed > 0 else 0
                pct = 100.0 * bytes_written / total_bytes
                remaining = (total_bytes - bytes_written) / rate if rate > 0 else 0
                sys.stdout.write(
                    "\r    [%5.1f%%] %6d / %d bytes  %5.0f B/s  ETA %ds   " % (
                        pct, bytes_written, total_bytes, rate, remaining))
                sys.stdout.flush()
            else:   # skipped packet, address needs to be set
                addr_set = 0

            offs = offs + trsf_size
            addr = addr + trsf_size
            lng = lng - trsf_size

        self.cmdDownload(addr, lng)
        result = self.cmdSendData(data[offs:offs+lng])
        bytes_written += lng

        elapsed = time.time() - write_start
        rate = bytes_written / elapsed if elapsed > 0 else 0
        sys.stdout.write(
            "\r    [100.0%%] %d / %d bytes  %5.0f B/s  total %.1fs\n" % (
                bytes_written, total_bytes, rate, elapsed))
        sys.stdout.flush()
        return result


class CC2340R5:
    def __init__(self, command_interface):
        self.command_interface = command_interface
        self.flash_start_addr = 0x00000000
        self.ccfg_start_addr = 0x4E020000

    def placeholder():
        return

    def erase(self):
        return self.command_interface.cmdBankErase()

    has_cmd_set_xosc = False

class CC27xx:
    def __init__(self, command_interface):
        self.command_interface = command_interface
        self.flash_start_addr = 0x00000000
        self.ccfg_start_addr = 0x4E020000
        self.scfg_start_addr = 0x4e040000

    def placeholder():
        return

    def erase(self):
        return self.command_interface.cmdBankErase()

    has_cmd_set_xosc = False

def query_yes_no(question, default="yes"):
    valid = {"yes": True,
             "y": True,
             "ye": True,
             "no": False,
             "n": False}
    if default == None:
        prompt = " [y/n] "
    elif default == "yes":
        prompt = " [Y/n] "
    elif default == "no":
        prompt = " [y/N] "
    else:
        raise ValueError("invalid default answer: '%s'" % default)

    while True:
        sys.stdout.write(question + prompt)
        choice = input().lower()
        if default != None and choice == '':
            return valid[default]
        elif choice in valid:
            return valid[choice]
        else:
            sys.stdout.write("Please respond with 'yes' or 'no' "
                             "(or 'y' or 'n').\n")


# Convert the entered IEEE address into an integer
def parse_ieee_address(inaddr):
    try:
        return int(inaddr, 16)
    except ValueError:
        # inaddr is not a hex string, look for other formats
        if ':' in inaddr:
            bytes = inaddr.split(':')
        elif '-' in inaddr:
            bytes = inaddr.split('-')
        if len(bytes) != 8:
            raise ValueError("Supplied IEEE address does not contain 8 bytes")
        addr = 0
        for i, b in zip(range(8), bytes):
            try:
                addr += int(b, 16) << (56-(i*8))
            except ValueError:
                raise ValueError("IEEE address contains invalid bytes")
        return addr


def _parse_range_values(device, values):
    if len(values) and len(values) < 3:
        page_addr_range = []
        try:
            for value in values:
                try:
                    if int(value) % int(device.page_size) != 0:
                        raise ValueError("Supplied addresses are not page_size: "
                                         "{} aligned".format(device.page_size))
                    page_addr_range.append(int(value))
                except ValueError:
                    if int(value, 16) % int(device.page_size) != 0:
                        raise ValueError("Supplied addresses are not page_size: "
                                         "{} aligned".format(device.page_size))
                    page_addr_range.append(int(value, 16))
            return page_addr_range
        except ValueError:
            raise ValueError("Supplied value is not a page or an address")
    else:
        raise ValueError("Supplied range is neither a page or address range")


def parse_page_address_range(device, pg_range):
    """Convert the address/page range into a start address and byte length"""
    values = pg_range.split(',')
    page_addr = []
    # check if first argument is character
    if values[0].isalpha():
        values[0].lower()
        if values[0] == 'p' or values[0] == 'page':
            if values[0] == 'p':
                values[1:] = device.page_to_addr(values[1:])
        elif values[0] != 'a' and values[0] != 'address':
            raise ValueError("Prefix is neither a(address) or p(page)")
        page_addr.extend(_parse_range_values(device, values[1:]))
    else:
        page_addr.extend(_parse_range_values(device, values))
    if len(page_addr) == 1:
        return [page_addr[0], device.page_size]
    else:
        return [page_addr[0], (page_addr[1] - page_addr[0])]


def print_version():
    # Get the version using "git describe".
    try:
        p = Popen(['git', 'describe', '--tags', '--match', '[0-9]*'],
                  stdout=PIPE, stderr=PIPE)
        p.stderr.close()
        line = p.stdout.readlines()[0]
        version = line.decode('utf-8').strip()
    except:
        # We're not in a git repo, or git failed, use fixed version string.
        version = __version__
    print('%s %s' % (sys.argv[0], version))

def get_devices():
    for d in SUPPORTED_DEVICES:
        print(d)

def usage():
    print("""\nUsage: %s [-DhqVfewvr] [-p port] [-d device] [-b baud] [-a addr] \
    [file.bin]
    -h, --help                   This help
    -q                           Quiet
    -V                           Verbose
    -e                           Mass erase
    -d device ID                 Device ID; must be included, case insensitive
                                 eg: -d cc2745r10q1
    --get-devices                Print supported device IDs 
    -w                           Write
    -W, --erase-write            Write after erasing section to write to (avoids
                                 a mass erase). Rounds up section to erase if
                                 not page aligned.
    -v                           Verify (CRC32 check)
    -p port                      Serial port (default: first USB-like port in /dev)
    -b baud                      Baud speed (default: 500000)
    -a addr                      Target address
    --version                    Print script version

Examples:
    ./%s -e -b 115200 -d cc2745r10q1 -6 -p COM112
    ./%s -d cc2745r10q1 -p COM112 -W example/main.hex

    """ % (sys.argv[0], sys.argv[0], sys.argv[0], sys.argv[0]))

class AddressBinary(object):
    def __init__(self, addr:int, filePath:str):
        self.addr = addr
        self.filePath = filePath
    
    def __str__(self):
        return "0x%08x %s" % (self.addr, self.filePath)
    def __repr__(self):
        return self.__str__()

if __name__ == "__main__":

    conf = {
            'port': 'auto',
            'device': '',  
            'baud': 500000,
            'address': None,
            'erase': 0,
            'write': 0,
            'erase_write': 0,
            'verify': 0,
            'multi_address_binaries': [],
        }

# http://www.python.org/doc/2.5.2/lib/module-getopt.html

    try:
        opts, args = getopt.getopt(sys.argv[1:],
                                   "hqVed:wWvp:b:a:",
                                   ['help', 'erase-write=',
                                    'erase-page=',
                                    'version', 'get-devices',
                                    'multi-address-binaries=',
                                    'hex-file='])
    except getopt.GetoptError as err:
        # print help information and exit:
        print(str(err))  # will print something like "option -a not recognized"
        usage()
        sys.exit(2)
    
    print(f"main: opts = {opts}, args = {args}")

    for o, a in opts:
        if False:
            mdebug(10, "    main: o = %s, a = %s" % (o, a)) 
        elif o == '-h' or o == '--help':
            usage()
            sys.exit(0)
        elif o == '--version':
            print_version()
            sys.exit(0)
        elif o == '-q':
            QUIET = 0
        elif o == '-V':
            QUIET = 10
        elif o == '-e':
            conf['erase'] = 1
        elif o == '-w':
            conf['write'] = 1
        elif o == '-W' or o == '--erase-write':
            conf['erase_write'] = 1
        elif o == '-v':
            conf['verify'] = 1
        elif o == '-p':
            conf['port'] = a
        elif o == '-d':
            conf['device'] = a
        elif o == '-b':
            conf['baud'] = eval(a)
        elif o == '-a':
            conf['address'] = eval(a)
        elif o == '--get-devices':
            get_devices()
            sys.exit(0)
        elif o == '--multi-address-binaries':
            for s in a.split(';'):
                print(f"toby:  main: s = {s}, size = {len(s)}")
                ab = s.split(',')
                conf['multi_address_binaries'].append(
                    AddressBinary(int(ab[0], 0), ab[1])
                )
            # conf['multi_address_binaries'].extend(
            #     AddressBinary(int(a, 0), f)
            #     for ab in a.split(',')
            #     for a, f in [ab.split(':')]
            # )
            print(conf['multi_address_binaries'])
            # sys.exit(0)
        elif o == '--hex-file':
            if not have_hex_support:
                assert False, "no hex support"
        else:
            assert False, "Unhandled option"
    
    try:
        # Sanity checks
        # Check for proper device type input
        if conf['device'] == '':
            raise Exception('No device ID given.')
        else:
            device_id_str = SUPPORTED_DEVICES.get(conf['device'].casefold(), None)
            if device_id_str == 'CCX2':
                DEVICE_TYPE = 'CCX2'
            elif device_id_str == 'CCX4':
                DEVICE_TYPE = 'CCX4'
            elif device_id_str == 'CC23XX':
                DEVICE_TYPE = 'CC23XX'
            elif device_id_str == 'CC27XXX10':
                DEVICE_TYPE = 'CC27XXX10'
            else:
                device_input = conf['device']
                print('ERROR: The device type \'%s\' is incorrect or not supported.'
                                ' Please use one of the currently supported device types' % device_input)
                get_devices()
                sys.exit(0)

        # check for input/output file
        if conf['write'] or conf['erase_write'] or conf['verify']:
            try:
                args[0]
            except:
                raise Exception('No file path given.')

        # Try and find the port automatically
        if conf['port'] == 'auto':
            ports = []

            # Get a list of all USB-like names in /dev
            for name in ['ttyACM',
                         'tty.usbserial',
                         'ttyUSB',
                         'tty.usbmodem',
                         'tty.SLAB_USBtoUART']:
                ports.extend(glob.glob('/dev/%s*' % name))

            ports = sorted(ports)

            if ports:
                # Found something - take it
                conf['port'] = ports[0]
            else:
                raise Exception('No serial port found.')

        cmd = CommandInterfaceF3()
        cmd.open(conf['port'], conf['baud'])
        mdebug(5, "Opening port %(port)s, baud %(baud)d"
               % {'port': conf['port'], 'baud': conf['baud']})
        if conf['write'] or conf['erase_write'] or conf['verify']:
            mdebug(5, "Reading data from %s" % args[0])
            # consider that this could be a string with comma separated file paths, files to be processed in order
            # actually, if getopts is used, then the remaining string(s) are in args
            # they indicate file paths to be processed
            firmware = []
            for fw_path in args:
                firmware.append(FirmwareFile(fw_path)) 

        mdebug(5, "Connecting to target...")

        if not cmd.sendSynch():
            raise CmdException("Can't connect to target. Ensure boot loader "
                               "is started. (no answer on synch sequence)")

        mdebug(10, "    main: attempt to cmdGetChipId")
        chip_id = cmd.cmdGetChipId()
        mdebug(10, "    main: chip id bytes to use: 0x%4x" % chip_id)
        chip_id_str = CHIP_ID_STRS.get(chip_id, None)
        mdebug(10, "    main: chip_id_str is " + str(chip_id_str))

        # TODO: come up with a better way to map chip_id to device
        if chip_id_str == 'CC2340':
            device = CC2340R5(cmd)
        elif chip_id_str == 'CC2745R10Q1':
            device = CC27xx(cmd)
        else:
            mdebug(10, "    main: Unknown chip id, aborting")
            sys.exit(1)

        # Choose a good default address unless the user specified -a OR if there is no write
        if conf['address'] == None and (conf['write'] or conf['erase_write']):
            mdebug(10, "    main: using default application flash start address: 0x%x" % device.flash_start_addr)
            conf['address'] = device.flash_start_addr

        if conf['erase']:
            mdebug(5, "    Performing mass erase")
            if device.erase():
                mdebug(5, "    Erase done")
            else:
                raise CmdException("Erase failed")

        if len(conf['multi_address_binaries']) > 0:
            # erase is needed, make sure user passed it is -- if not, erase here
            if conf['erase'] == 0:
                mdebug(5, "    Performing mass erase")
                if device.erase():
                    mdebug(5, "    Erase done")
                else:
                    raise CmdException("Erase failed")
            for ab in conf['multi_address_binaries']:
                ab: AddressBinary
                if cmd.writeMemory(ab.addr, FirmwareFile(ab.filePath).bytes):
                    mdebug(5, "    Write done                                ")
                else:
                    raise CmdException("Write failed                       ")
        elif conf['write']:
            # erase is needed, make sure user passed it is -- if not, erase here
            if conf['erase'] == 0:
                mdebug(5, "    Performing mass erase")
                if device.erase():
                    mdebug(5, "    Erase done")
                else:
                    raise CmdException("Erase failed")
            # TODO: check if boot loader back-door is open, need to read
            #       flash size first to get address
            for fw in firmware:
                for seg_addr, seg_data in fw.segments:
                    addr = seg_addr if conf['address'] is None else (conf['address'] if seg_addr == 0 else seg_addr)
                    flash_start = time.time()
                    if cmd.writeMemory(addr, seg_data):
                        mdebug(5, "    Write done 0x%08X (%.1fs)" % (addr, time.time() - flash_start))
                    else:
                        raise CmdException("Write failed at 0x%08X" % addr)

        if conf['erase_write']:
            # erase is needed, make sure user passed it is -- if not, erase here
            if conf['erase'] == 0:
                mdebug(5, "    Performing mass erase")
                if device.erase():
                    mdebug(5, "    Erase done")
                else:
                    raise CmdException("Erase failed")

            for fw in firmware:
                for seg_addr, seg_data in fw.segments:
                    addr = seg_addr if conf['address'] is None else (conf['address'] if seg_addr == 0 else seg_addr)
                    flash_start = time.time()
                    if cmd.writeMemory(addr, seg_data):
                        mdebug(5, "    Write done 0x%08X (%.1fs)" % (addr, time.time() - flash_start))
                    else:
                        raise CmdException("Write failed at 0x%08X" % addr)

        if conf['verify']:
            mdebug(5, "Verifying by comparing CRC32 calculations.")

            crc_local = firmware.crc32()
            # CRC of target will change according to length input file
            crc_target = device.crc(conf['address'], len(firmware.bytes))

            if crc_local == crc_target:
                mdebug(5, "    Verified (match: 0x%08x)" % crc_local)
            else:
                cmd.cmdReset()
                raise Exception("NO CRC32 match: Local = 0x%x, "
                                "Target = 0x%x" % (crc_local, crc_target))

        cmd.cmdReset()

    except Exception as err:
        if QUIET >= 10:
            traceback.print_exc()
        exit('ERROR: %s' % str(err))
