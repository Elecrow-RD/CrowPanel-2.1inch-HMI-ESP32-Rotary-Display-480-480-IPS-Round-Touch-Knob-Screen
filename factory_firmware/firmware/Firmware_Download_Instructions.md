# RotaryScreen 2.1 Firmware Download Instructions

## Firmware Version

- Target chip: ESP32-S3
- Flash: 16 MB
- Partition scheme: `elecrow_s3`
- USB mode: Hardware CDC
- Upload baud rate: 921600
- Included in this version: fix for false triggers during fast knob rotation, touch swipe direction adjustment, and fix for ghosting when switching pages

## Recommended File

The merged firmware below is recommended — a single flash is all you need:

`RotaryScreen_2_1_20260915_no_transition_merged.bin`

This file is a full 16 MB image, and the flash address is fixed at `0x0`.

## Flashing with esptool

1. Connect the device with a USB data cable and make sure it enters download mode.
2. Check the port number in Windows Device Manager, for example `COM12`.
3. Open PowerShell and navigate to the `firmware_build` folder in this directory.
4. Run the command below, replacing `COM12` with your actual port number:

```powershell
esptool.exe --chip esp32s3 --port COM12 --baud 921600 write_flash -z 0x0 RotaryScreen_2_1_20260915_no_transition_merged.bin
```

If the system cannot find `esptool.exe`, use the path bundled with the Arduino ESP32 core:

```powershell
& "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py\5.2.0-cn\esptool.exe" --chip esp32s3 --port COM12 --baud 921600 write_flash -z 0x0 RotaryScreen_2_1_20260915_no_transition_merged.bin
```

After flashing is complete, press the reset button once. On first boot, it is recommended to open the serial monitor with the baud rate set to `115200`.

## Flashing Individual Files

If your flashing tool does not accept a merged image, you can use the following files and addresses:

| File | Address |
|---|---:|
| `RotaryScreen_2_1_20260915_bootloader.bin` | `0x0000` |
| `RotaryScreen_2_1_20260915_partitions.bin` | `0x8000` |
| `RotaryScreen_2_1_20260915_boot_app0.bin` | `0xE000` |
| `RotaryScreen_2_1_20260915_app.bin` | `0x10000` |

The command is as follows:

```powershell
esptool.exe --chip esp32s3 --port COM12 --baud 921600 write_flash -z `
  0x0000 RotaryScreen_2_1_20260915_bootloader.bin `
  0x8000 RotaryScreen_2_1_20260915_partitions.bin `
  0xE000 RotaryScreen_2_1_20260915_boot_app0.bin `
  0x10000 RotaryScreen_2_1_20260915_app.bin
```

## If Flashing Fails

- Make sure you are using a data cable, not a USB cable that only supports charging.
- Close the Arduino IDE serial monitor and any other program occupying that COM port.
- Hold down the BOOT button, press RESET once, release RESET, then release BOOT, and run the flashing command again.
- If it still will not start, consider running `erase_flash` first; this will erase all Flash data on the device, so use it with caution.

## SHA-256 Checksums

```text
RotaryScreen_2_1_20260915_no_transition_merged.bin  4D7E7545FA1D890B28C720807A7884A87A51F78FE5E3AD9A1E5A0EEB4E062AEA
RotaryScreen_2_1_20260915_app.bin                    C2E1DCB55745E2DE8B076CCA141BB5674C6C96A493666894E97A55C9713A810A
RotaryScreen_2_1_20260915_bootloader.bin             B68C1ED33C43A4290375213F24D608C125E31E22880D16D4A5DCBA89E083716D
RotaryScreen_2_1_20260915_partitions.bin             0B9C3FF6810D66DB6F419056F0FE49B118D8CF251049677E9475FAE3FD0FB5D3
RotaryScreen_2_1_20260915_boot_app0.bin              F94C5D786A7A8FAB06AC5D10E33BF37711A6697636DC037559EA19CC410A17F0
```

PowerShell verification command:

```powershell
Get-FileHash .\RotaryScreen_2_1_20260915_no_transition_merged.bin -Algorithm SHA256
```
