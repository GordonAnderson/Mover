// Mover is designed to be located in flash bank 1. When this application starts it will move
// The application located at APP_SRC_ADDRESS to APP_DST_ADDRESS. After the file is moved and
// verified Mover will force a system reboot to start the application.
//
// Gordon Anderson
//

#include <DueFlashStorage.h>
#include <efc.h>
#include <flash_efc.h>
#include "Arduino.h"

#define APP_SRC_ADDRESS (IFLASH1_ADDR + 0x10000)
#define APP_DST_ADDRESS IFLASH0_ADDR
#define APP_LENGTH 0x30000

DueFlashStorage dueFlashStorage;

__attribute__ ((long_call, section (".ramfunc")))
void setupForReboot()
{
  __disable_irq();

//Adapted from code found in Reset.cpp in Arduino core files for Due
//GPNVM bits are as follows:
//0 = lock bit (1 = flash memory is security locked)
//1 = Booting mode (0 = boot from ROM, 1 = boot from FLASH)
//2 = Flash ordering (0 = normal ordering FLASH0 then FLASH1, 1 = Reverse so FLASH1 is mapped first)
    
  const int EEFC_FCMD_CGPB = 0x0C;
  const int EEFC_FCMD_SGPB = 0x0B;
  const int EEFC_KEY = 0x5A;
  while ((EFC0->EEFC_FSR & EEFC_FSR_FRDY) == 0);
  // Set bootflag to run from FLASH instead of ROM
  EFC0->EEFC_FCR =
    EEFC_FCR_FCMD(EEFC_FCMD_SGPB) |
    EEFC_FCR_FARG(1) |
    EEFC_FCR_FKEY(EEFC_KEY);
  while ((EFC0->EEFC_FSR & EEFC_FSR_FRDY) == 0);  
  // Set bootflag to run from FLASH0
  EFC0->EEFC_FCR =
    EEFC_FCR_FCMD(EEFC_FCMD_CGPB) |
    EEFC_FCR_FARG(2) |
    EEFC_FCR_FKEY(EEFC_KEY);
  while ((EFC0->EEFC_FSR & EEFC_FSR_FRDY) == 0);  

  // Force a hard reset
  const int RSTC_KEY = 0xA5;
  RSTC->RSTC_CR =
    RSTC_CR_KEY(RSTC_KEY) |
    RSTC_CR_PROCRST |
    RSTC_CR_PERRST;

  while (true); //bye cruel world!
}

void setup() 
{
  byte     *src,*d;
  uint32_t dst,i;

  src = (byte *)APP_SRC_ADDRESS;
  d = (byte *)APP_DST_ADDRESS;
  dst = APP_DST_ADDRESS;
  delay(100);
  SerialUSB.begin(115200);
  SerialUSB.println("Mover R1.0 February 25, 2018");
  // Move the application one page at a time. Assume the application is
  // located at 0xD0000. Move code from 0xD0000 to 0x80000, lenght of 0x30000
  for(i = 0; i < APP_LENGTH; i += 256)
  {
    noInterrupts();
    dueFlashStorage.writeAbs(dst + i, &src[i], 256);
  }
  interrupts();
  // Verify the move
  for(i = 0; i < APP_LENGTH; i += 256)
  {
    if(src[i] != d[i])
    {
      // Here with verify error so we have an issue!
      while(1)
      {
        delay(2000);
        SerialUSB.println("Verify error on move!");
      }
    }
  }
  setupForReboot();
}

void loop() 
{
  // Reset the processor, should never get here!
  setupForReboot();
}
