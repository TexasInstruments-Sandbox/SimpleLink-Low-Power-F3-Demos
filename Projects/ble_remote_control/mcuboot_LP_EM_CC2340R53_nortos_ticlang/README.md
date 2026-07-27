# MCUBoot Bootloader — CC2340R53

Secure bootloader for the BLE Remote Control firmware. Runs at reset before the application, verifies the ECDSA P-256 signature of the RC firmware image, and boots it if valid. Also manages OAD (Over-the-Air Download) slot copying in overwrite mode.

---

## Dependencies

- TI Device: CC2340R53
- TI SDK: simplelink_lowpower_f3_sdk_9_12_00_19
- TI Code Composer Studio: CCS Theia 20.5.0
- SysConfig: 1.23.2
- Compiler: TI CLANG 4.0.4.LTS
- RTOS: NoRTOS

---

## Flash Memory Layout

The CC2340R53 has 512 KB of internal flash. MCUBoot occupies the bottom, followed by two equally sized application slots.

| Region | Start Address | Size | Description |
|--------|--------------|------|-------------|
| MCUBoot | `0x00000000` | 24 KB (`0x6000`) | Bootloader — always at address 0 |
| Primary Slot | `0x00006000` | 212 KB (`0x35000`) | Active application image |
| Secondary Slot | `0x0003B000` | 212 KB (`0x35000`) | OAD candidate image |

The RC firmware image must be built with header offset `0x6000` and slot size `0x35000` to match this layout.

---

## Configuration

### Upgrade Mode

Configured as **overwrite-only** (`MCUBOOT_OVERWRITE_ONLY`). On a valid OAD update:
1. MCUBoot copies the candidate image from the secondary slot into the primary slot.
2. The secondary slot is erased.
3. MCUBoot boots the newly copied image.

Swap-based upgrade is not used. Downgrade prevention is enabled (`MCUBOOT_DOWNGRADE_PREVENTION`) — the candidate image version must be greater than or equal to the current image version.

### Image Verification

- Algorithm: **ECDSA P-256** (`MCUBOOT_SIGN_EC256`)
- Crypto engine: TI hardware crypto (`MCUBOOT_USE_TI_CRYPTO`)
- MCUBoot always validates the primary slot signature before booting (`MCUBOOT_VALIDATE_PRIMARY_SLOT`).
- An unsigned or incorrectly signed image will not boot.

### Public Key

The public key used to verify application images is compiled into MCUBoot via `keys.c`, linked from the SDK:

```
$SDK/source/third_party/mcuboot/boot/ti/source/mcuboot_app/keys.c
```

The matching private key (TI sample key, **replace in production**):

```
$SDK/source/third_party/mcuboot/root-ec-p256.pem
```

To use your own key pair, see the [RC firmware README — OAD section](../ble_remote_control_CC2340R53_freertos_ticlang/README.md) for the full key replacement and imgtool signing procedure.

### SysConfig Settings

| Setting | Value | Notes |
|---------|-------|-------|
| Upgrade mode | Overwrite | Single-copy, no swap |
| Signature | ECDSA P-256 | EC256 with TI hardware crypto |
| Primary slot base | `0x6000` | Immediately after bootloader |
| Primary slot size | `0x35000` | 212 KB |
| Secondary slot base | `0x3B000` | Immediately after primary |
| Secondary slot size | `0x35000` | 212 KB |
| LF clock source | LF RCOSC | Internal oscillator, no external crystal |
| Voltage regulator | GLDO | Low-dropout regulator |
| Bootloader access | Forbidden | `bldrSetting = Any bootloader forbidden` |

---

## Flashing with UniFlash

Both MCUBoot and the RC application firmware must be flashed together. They occupy non-overlapping address regions and can be loaded in a single UniFlash session.

### Files to Flash

| File | Address | Description |
|------|---------|-------------|
| `mcuboot_LP_EM_CC2340R53_nortos_ticlang.hex` | `0x00000000` | MCUBoot bootloader |
| `ble_remote_control_CC2340R53_freertos_ticlang.hex` | `0x00006000` | RC application (signed, with MCUBoot header) |

Pre-built binaries are located in their respective build output folders:
- `mcuboot_LP_EM_CC2340R53_nortos_ticlang/Debug/mcuboot_LP_EM_CC2340R53_nortos_ticlang.hex`
- `ble_remote_control_CC2340R53_freertos_ticlang/Release/ble_remote_control_CC2340R53_freertos_ticlang.hex`

### UniFlash Steps

1. Open **UniFlash** and select device **CC2340R53**.
2. Connect the board via XDS110 (USB).
3. In the **Program** tab, click **Add file** and load the MCUBoot `.hex`.
4. Click **Add file** again and load the RC firmware `.hex`.
5. Verify both files appear in the file list with correct addresses.
6. Ensure **Erase** is set to **Erase sectors needed by files** (not full erase, to preserve NVS if desired; or full erase for a clean flash).
7. Click **Load Image**.
8. After flashing completes, power-cycle the board.

> **Important:** MCUBoot must always be present at `0x00000000`. Flashing only the RC application without MCUBoot (or overwriting address `0x0`) will result in a non-booting device.

> **Important:** The RC firmware `.hex` must be a **signed image** with a valid MCUBoot header at offset `0x6000`. Images built from source using the default post-build imgtool step are already signed with the TI sample key. If you have replaced the key, ensure the image is signed with the corresponding private key before flashing.

---

## Building from Source

1. Open CCS Theia and import the `mcuboot_LP_EM_CC2340R53_nortos_ticlang` project.
2. Build — the post-build step automatically converts the ELF to `.hex`:
   ```
   tiarmhex -order MS --memwidth=8 --romwidth=8 --intel -o mcuboot_LP_EM_CC2340R53_nortos_ticlang.hex mcuboot_LP_EM_CC2340R53_nortos_ticlang.out
   ```
3. The output `.hex` is placed in the `Debug/` folder.

---

## References

- [BLE5-Stack User's Guide — OAD](https://software-dl.ti.com/simplelink/esd/simplelink_lowpower_f3_sdk/9.12.00.19/exports/docs/ble5stack/ble_user_guide/html/ble-stack-5.x-guide/index-cc23xx.html)
- [MCUBoot Documentation](https://docs.mcuboot.com/)
- [UniFlash](https://www.ti.com/tool/UNIFLASH)
