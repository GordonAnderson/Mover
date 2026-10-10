//
// Mover
//
// Field update helper for the ARB module. Mover lives in FLASH bank 1 at 0xC0000. The ARB
// application receives a new firmware image with the ARBPGM command, which stages it at
// APP_SRC_ADDRESS (bank 1). The MOVEIT command then boots the processor from bank 1, which
// runs this program. Mover copies the staged image to APP_DST_ADDRESS (bank 0, where the
// ARB application runs from), verifies it, selects bank 0 as the boot bank and resets.
//
// If the staged image does not look like a valid application, bank 0 is left untouched and
// the processor reboots into the existing application.
//
// Gordon Anderson
//
// Revision history
//  R1.0, Feb 25, 2018   Initial version
//  R1.1, Oct 10, 2026   PlatformIO port. Verify now checks every byte (it only compared the
//                       first byte of each page), write errors are checked and retried, the
//                       staged image is validated before bank 0 is erased, and the copy length
//                       no longer includes the saved settings area.
//
#include <Arduino.h>
#include <DueFlashStorage.h>

#define MOVER_VERSION "Mover R1.1 October 10, 2026"

// Memory map, must agree with ARB2 (ProgramFLASH() and NonVolStorage in ARB2.h)
#define APP_SRC_ADDRESS   (IFLASH1_ADDR + 0x10000)            // 0xD0000, staged image
#define APP_DST_ADDRESS   IFLASH0_ADDR                        // 0x80000, running application
#define NONVOL_STORAGE    (IFLASH1_ADDR + IFLASH1_SIZE - 1000) // ARB settings, never copied
#define PAGE_SIZE         256u
// Whole pages between the staged image and the settings area
#define APP_LENGTH        (((NONVOL_STORAGE - APP_SRC_ADDRESS) / PAGE_SIZE) * PAGE_SIZE)

#define COPY_ATTEMPTS     3
#define ERROR_REPORT_MS   2000

DueFlashStorage dueFlashStorage;

// Selects FLASH bank 0 as the boot bank and resets the processor. Runs from RAM because it
// changes the FLASH controller settings while executing from FLASH. Never returns.
//
// GPNVM bits:
//   0 = security lock bit
//   1 = boot mode (0 = ROM, 1 = FLASH)
//   2 = boot flash (0 = FLASH0, 1 = FLASH1)
// Adapted from Reset.cpp in the Arduino Due core.
__attribute__ ((long_call, noinline, section (".ramfunc")))
static void bootFromFlash0()
{
  const uint32_t EEFC_FCMD_CGPB = 0x0C;   // Clear GPNVM bit
  const uint32_t EEFC_FCMD_SGPB = 0x0B;   // Set GPNVM bit
  const uint32_t EEFC_KEY       = 0x5A;
  const uint32_t RSTC_KEY       = 0xA5;

  __disable_irq();
  while ((EFC0->EEFC_FSR & EEFC_FSR_FRDY) == 0);
  // Boot from FLASH instead of ROM
  EFC0->EEFC_FCR = EEFC_FCR_FCMD(EEFC_FCMD_SGPB) | EEFC_FCR_FARG(1) | EEFC_FCR_FKEY(EEFC_KEY);
  while ((EFC0->EEFC_FSR & EEFC_FSR_FRDY) == 0);
  // Boot from FLASH0
  EFC0->EEFC_FCR = EEFC_FCR_FCMD(EEFC_FCMD_CGPB) | EEFC_FCR_FARG(2) | EEFC_FCR_FKEY(EEFC_KEY);
  while ((EFC0->EEFC_FSR & EEFC_FSR_FRDY) == 0);
  // Reset the processor and peripherals
  RSTC->RSTC_CR = RSTC_CR_KEY(RSTC_KEY) | RSTC_CR_PROCRST | RSTC_CR_PERRST;
  while (true);
}

// Basic check of the staged image: the first two vector table entries are the initial stack
// pointer (must be in SRAM) and the reset handler (Thumb address inside FLASH bank 0).
static bool stagedImageIsValid()
{
  const uint32_t *vectors = (const uint32_t *)APP_SRC_ADDRESS;
  uint32_t sp    = vectors[0];
  uint32_t reset = vectors[1];

  if (sp < IRAM0_ADDR || sp > IRAM0_ADDR + 0x100000) return false;
  if (!(reset & 1)) return false;
  reset &= ~1u;
  return reset >= IFLASH0_ADDR && reset < IFLASH0_ADDR + IFLASH0_SIZE;
}

// Copies the staged image to bank 0 one page at a time, verifying each page.
static bool copyImage()
{
  const uint8_t *src = (const uint8_t *)APP_SRC_ADDRESS;
  const uint8_t *dst = (const uint8_t *)APP_DST_ADDRESS;

  for (uint32_t i = 0; i < APP_LENGTH; i += PAGE_SIZE)
  {
    noInterrupts();
    bool ok = dueFlashStorage.writeAbs(APP_DST_ADDRESS + i, (byte *)&src[i], PAGE_SIZE);
    interrupts();
    if (!ok || memcmp(&src[i], &dst[i], PAGE_SIZE) != 0) return false;
  }
  return true;
}

// Called with a failure that leaves bank 0 in an unknown state. Resetting would just run
// this program again, so report the problem and wait for a power cycle or a new upload.
static void fatal(const char *msg)
{
  while (true)
  {
    SerialUSB.println(msg);
    delay(ERROR_REPORT_MS);
  }
}

void setup()
{
  delay(100);
  SerialUSB.begin(115200);
  SerialUSB.println(MOVER_VERSION);

  if (!stagedImageIsValid())
  {
    SerialUSB.println("No valid application at the staging address, application not changed.");
    delay(100);
    bootFromFlash0();
  }
  for (int attempt = 0; attempt < COPY_ATTEMPTS; attempt++)
  {
    if (copyImage())
    {
      SerialUSB.println("Application moved and verified.");
      delay(100);
      bootFromFlash0();
    }
    SerialUSB.println("Move failed, retrying.");
  }
  fatal("Verify error on move! Application in FLASH0 is corrupt, upload again.");
}

void loop()
{
  // Should never get here
  bootFromFlash0();
}
