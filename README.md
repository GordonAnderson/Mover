# Mover

Field-update helper for the GAA Custom Electronics **ARB module** (see the `ARB2` project).
It lets the ARB firmware be replaced from the MIPS host application over the USB serial
connection, with no programmer and no SAM-BA.

Current version: **R1.1**. Open items are in [TODO.md](TODO.md).

## The whole story

Three programs are involved:

| Program | Where | Role |
|---|---|---|
| **ARB2** firmware (`../ARB2`) | FLASH bank 0, `0x80000` | The running application. Provides the `ARBPGM` and `MOVEIT` serial commands. |
| **Mover** (this project) | FLASH bank 1, `0xC0000` | Copies a new ARB image into bank 0. Runs only during an update. |
| **MIPS Qt host app** (`MIPSapp/MIPS_QT6`) | PC | *ARB upload* menu action, which sends files to the ARB with `ARBPGM`. |

A SAM3X8E has two 256 KB FLASH banks. The ARB application is linked to run from bank 0.
Bank 1 holds Mover, a staging area for the new image, and the ARB's saved settings:

| Address | Size | Contents |
|---|---|---|
| `0x80000` – `0xBFFFF` | 256 KB | Bank 0: the running ARB application |
| `0xC0000` – `0xCFFFF` | 64 KB | Bank 1: **Mover** (actual size about 12 KB) |
| `0xD0000` – `0xFFC17` | about 191 KB | Bank 1: staged new ARB image |
| `0xFFC18` – `0xFFFFF` | 1000 bytes | Bank 1: ARB saved settings (`NonVolStorage`), never overwritten or copied |

### One-time setup of a module (install Mover)

Mover is not part of the ARB firmware. It is uploaded once, using the ARB's own `ARBPGM`
command, so no programmer is needed:

1. Build this project (`pio run`). The build copies the result to `firmware/Mover-R<ver>.bin`
   (the version comes from `MOVER_VERSION` in `src/Mover.cpp`).
2. In the MIPS app, connect to the ARB module and choose **ARB upload**.
3. Enter the address `c0000` and select the `Mover-R<ver>.bin` file.

The ARB firmware must be a version that has `ARBPGM` and `MOVEIT` (ARB 1.12 or later, and
ARB2 all versions). The Mover image has to stay under 64 KB or it would run into the staging
area. The linker script makes the build fail if it does.

> **Do not run `MOVEIT` on a module that does not have Mover installed.** Bank 1 would be
> blank, the processor would boot into nothing, and the module could then only be recovered
> through SAM-BA with the erase jumper.

### Updating the ARB firmware in the field

1. Build ARB2 (`pio run` in `../ARB2`). The firmware is copied to `firmware/ARB2-V<ver>.bin`.
2. In the MIPS app, connect to the ARB module and choose **ARB upload**.
3. Enter the address `d0000` and select the ARB `.bin`. The app sends
   `ARBPGM,d0000,<size>`, then the file as hex text in 512-byte blocks, then an 8-bit CRC.
   The ARB writes it to the staging area in 256-byte pages and replies ACK if the CRC
   matches. The old application keeps running, so nothing is lost if the upload fails.
4. Send `MOVEIT,5410` from the MIPS terminal. The ARB sets the boot bits to boot from bank 1
   and resets.
5. Mover runs:
   1. It checks the staged image has a plausible vector table (stack pointer in SRAM, reset
      handler inside bank 0). If not, bank 0 is left alone and the ARB reboots into its
      old application.
   2. It copies the image to `0x80000` one 256-byte page at a time. Each page is read back
      and compared. The whole copy is retried up to 3 times.
   3. It sets the boot bits back to bank 0 and resets.
6. The new ARB application starts. Use `GVER` to confirm the version.

The progress is also printed on the native USB port at 115200 baud (`Mover R1.1 ...`,
`Application moved and verified.`).

If all three attempts fail, bank 0 is corrupt. Mover prints `Verify error on move!` every
2 seconds on the native USB port and stays there. Power cycling retries the move, because the
boot bits still point at bank 1 and the staged image is not erased. If it keeps failing, the
ARB application is not running, so `ARBPGM` is not available and the module has to be
reprogrammed over the native USB port with bossac (bank 0 at `0x80000`).

The staged copy stays in bank 1 after an update. A later `MOVEIT` with no new upload simply
reinstalls the same version.

## Building

Requires [PlatformIO](https://platformio.org). The board is `dueUSB` (native USB port).

```
pio run
```

The project uses its own linker script, [ld/mover_flash.ld](ld/mover_flash.ld), a copy of the
Arduino Due `flash.ld` with the FLASH origin moved to `0xC0000` and the length limited to
64 KB. [lib/DueFlashStorage](lib/DueFlashStorage) is a copy of the library used by ARB2.

## Keeping in sync with ARB2

These values are defined in both projects and must agree:

| Value | Mover.cpp | ARB2 |
|---|---|---|
| Staging address `0xD0000` | `APP_SRC_ADDRESS` | The MIPS app prompt text and the `ARBPGM` address typed by the user |
| Settings area (top 1000 bytes) | `NONVOL_STORAGE` | `NonVolStorage` in `include/ARB2.h` |
| ARB application address | `IFLASH0_ADDR` | `ld` script of the ARB build (stock `flash.ld`) |
| `MOVEIT` pass code | not used | 5410, `MoveIt()` in `src/Serial.cpp` |

`ARBPGM` only accepts addresses from `0xC0000` up to the settings area, so it cannot damage
the running application in bank 0.

## Files

- [src/Mover.cpp](src/Mover.cpp): the whole program
- [rename_firmware.py](rename_firmware.py): post-build step that writes `firmware/Mover-R<ver>.bin`
- [firmware/](firmware): versioned release builds
- [ld/mover_flash.ld](ld/mover_flash.ld): linker script for bank 1
- [lib/DueFlashStorage/](lib/DueFlashStorage): FLASH write library
