# Mover TODO

## Verify on hardware (R1.1)

R1.1 was built but has not been run on an ARB yet.

- [ ] **Install Mover with ARB upload.** Upload `firmware/Mover-R1.1.bin` at `c0000` from the MIPS app
      (documented in the README) and confirm it lands intact with `GETFLASH`.
- [ ] **Full update cycle.** *ARB upload* an ARB2 `.bin` at `d0000`, then `MOVEIT,5410`, and confirm the new
      version starts. Do it with the current 107 KB image and with one near the size limit.
- [ ] **Invalid image path.** Erase the staging area and run `MOVEIT`. The ARB should come
      back up on the old application.
- [ ] **Verify failure path.** Check the retry and the error message with a simulated write
      failure.
- [ ] **Stale data after a short image.** A smaller image leaves the tail of the previous
      upload in the staging area, and Mover copies it. Harmless, but it could be erased first.

## Make the update process fail-safe (planned)

Today a user error can brick a module: `MOVEIT` with no Mover installed, a wrong address
typed in the MIPS *ARB upload* dialog (for example overwriting Mover), or a corrupt or
wrong-product image. Planned changes, in priority order:

1. - [ ] **Remove the address from the user's hands.** Replace `ARBPGM,<addr>,<size>` with
      commands that have a fixed target: `ARBPGM` for the app (always `0xD0000`) and a separate
      guarded command for Mover. The MIPS dialog then only asks for a file. ARB2 `ARBPGM`
      must refuse any write that touches the Mover region (`0xC0000`-`0xCFFFF`). Needs ARB2
      and MIPS Qt app changes.
2. - [ ] **Image header and CRC32.** The post-build script appends a trailer with magic,
      product ID, version, length and CRC32. `ARBPGM` re-reads the staged image from flash and
      checks CRC32 and product ID. `MOVEIT` refuses unless the staged image is valid and Mover
      is present (magic and CRC at `0xC0000`). Mover checks the staged image again before it
      erases bank 0.
3. - [ ] **Make Mover the permanent boot loader.** Set the boot bits once to boot from bank 1,
      so Mover runs at every power-up and the GPNVM bits are never flipped back and forth.
      If an update is pending and valid it copies the image. It then checks the bank 0 app CRC
      and jumps to the app (set VTOR and MSP, branch). If the app is missing or corrupt it
      stays in the loader and accepts an upload over native USB using the same hex and CRC
      protocol, so a module is never dead, only waiting for firmware. Removes the `MOVEIT`
      boot-bit code from ARB2. Mover grows by a small serial receiver.
4. - [ ] **Protect Mover.** Set the SAM3X flash lock bits on the Mover pages, and run the
      watchdog during the copy so a hang resets and retries.
5. - [ ] **Last resort documentation.** Keep the SAM-BA erase jumper reachable and document
      the bossac recovery for service.

Rollback to the previous image was considered and rejected: bank 1 has about 191 KB free
after Mover, and the app is 107 KB, so only one image fits (rollback would need an app under
about 95 KB).

## Ideas

- [ ] `MOVEIT` in ARB2 should refuse to run when bank 1 does not hold a valid Mover (check the
      vector table at `0xC0000`), so a missing Mover cannot brick a module.
- [ ] The MIPS app does not send `MOVEIT` itself. Add it to the ARB upload action or a menu item.
- [ ] Add a CRC of the staged image (sent with `ARBPGM`) and check it before touching bank 0.
- [ ] The staged image is copied before it is known to be good. An A/B scheme with a
      signature word would remove the corrupt-bank-0 case.
- [ ] Share the memory map constants with ARB2 through one header instead of keeping two copies.

## Done

### R1.1 (PlatformIO port)
- Ported from the Arduino IDE. Added a linker script that places Mover at `0xC0000`.
- Verify only compared the first byte of each 256 byte page, so a bad write passed. It now
  compares every byte.
- The result of each FLASH write was ignored.
- A blank or invalid staged image was copied over the working application. It is now
  checked first and bank 0 is left alone if it is not valid.
- The copy length (0x30000) included the ARB settings area at the top of bank 1.
- Interrupts were disabled for the whole copy loop and re-enabled only at the end.
