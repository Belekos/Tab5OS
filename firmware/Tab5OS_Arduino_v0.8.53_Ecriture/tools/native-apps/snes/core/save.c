#include <stdio.h>
/* This file is part of Snes9x. See LICENSE file. */

#include "snes9x.h"
#include "memmap.h"
#include "ppu.h"
#include "cpuexec.h"
#include "apu.h"
#include "dma.h"
#include "display.h"
#include "srtc.h"
#include "soundux.h"

static const char header[16] = "TAB5SNES_STATE03";


bool S9xSaveState(const char *filename)
{
   SCPUState cpu = CPU;
   SICPU icpu = ICPU;
   SIAPU iapu = IAPU;
   SSoundData sound = SoundData;
   cpu.PC = cpu.PCBase = cpu.PCAtOpcodeStart = cpu.WaitAddress = NULL;
   icpu.UNUSED1 = NULL; icpu.S9xOpcodes = NULL;
   iapu.PC = (uint8_t *)(uintptr_t)(IAPU.PC - IAPU.RAM);
   iapu.DirectPage = (uint8_t *)(uintptr_t)(IAPU.DirectPage - IAPU.RAM);
   iapu.WaitAddress1 = IAPU.WaitAddress1 ? (uint8_t *)(uintptr_t)(IAPU.WaitAddress1 - IAPU.RAM + 1) : NULL;
   iapu.WaitAddress2 = IAPU.WaitAddress2 ? (uint8_t *)(uintptr_t)(IAPU.WaitAddress2 - IAPU.RAM + 1) : NULL;
   iapu.RAM = NULL;
   for (unsigned i = 0; i < NUM_CHANNELS; ++i) {
      sound.channels[i].block = NULL;
      sound.channels[i].echo_buf_ptr = NULL;
   }
   size_t chunks = 0;
   FILE *fp = NULL;

   if (!(fp = fopen(filename, "wb")))
      return false;

   chunks += fwrite(&header, sizeof(header), 1, fp);
   chunks += fwrite(&cpu, sizeof(CPU), 1, fp);
   chunks += fwrite(&icpu, sizeof(ICPU), 1, fp);
   chunks += fwrite(&PPU, sizeof(PPU), 1, fp);
   chunks += fwrite(&DMA, sizeof(DMA), 1, fp);
   chunks += fwrite(Memory.VRAM, VRAM_SIZE, 1, fp);
   chunks += fwrite(Memory.RAM, RAM_SIZE, 1, fp);
   chunks += fwrite(Memory.SRAM, SRAM_SIZE, 1, fp);
   chunks += fwrite(Memory.FillRAM, FILLRAM_SIZE, 1, fp);
   chunks += fwrite(&APU, sizeof(APU), 1, fp);
   chunks += fwrite(&iapu, sizeof(IAPU), 1, fp);
   chunks += fwrite(IAPU.RAM, 0x10000, 1, fp);
   chunks += fwrite(&sound, sizeof(SoundData), 1, fp);

   printf("Saved chunks = %d\n", chunks);

   int error = fclose(fp);

   return chunks == 13 && !error;
}

bool S9xLoadState(const char *filename)
{
   uint8_t buffer[512];
   size_t chunks = 0;
   FILE *fp = NULL;

   if (!(fp = fopen(filename, "rb")))
      return false;

   if (!fread(buffer, 16, 1, fp) || memcmp(header, buffer, sizeof(header)) != 0)
   {
      printf("Wrong header found\n");
      goto fail;
   }

   const long expected = 16 + sizeof(CPU) + sizeof(ICPU) + sizeof(PPU) + sizeof(DMA)
      + VRAM_SIZE + RAM_SIZE + SRAM_SIZE + FILLRAM_SIZE + sizeof(APU) + sizeof(IAPU)
      + 0x10000 + sizeof(SoundData);
   if (fseek(fp, 0, SEEK_END) || ftell(fp) != expected || fseek(fp, 16, SEEK_SET)) goto fail;
   S9xReset();

   uint8_t *IAPU_RAM = IAPU.RAM;
   SIAPU saved;

   chunks += fread(&CPU, sizeof(CPU), 1, fp);
   chunks += fread(&ICPU, sizeof(ICPU), 1, fp);
   chunks += fread(&PPU, sizeof(PPU), 1, fp);
   chunks += fread(&DMA, sizeof(DMA), 1, fp);
   chunks += fread(Memory.VRAM, VRAM_SIZE, 1, fp);
   chunks += fread(Memory.RAM, RAM_SIZE, 1, fp);
   chunks += fread(Memory.SRAM, SRAM_SIZE, 1, fp);
   chunks += fread(Memory.FillRAM, FILLRAM_SIZE, 1, fp);
   chunks += fread(&APU, sizeof(APU), 1, fp);
   chunks += fread(&saved, sizeof(saved), 1, fp);
   chunks += fread(IAPU_RAM, 0x10000, 1, fp);
   chunks += fread(&SoundData, sizeof(SoundData), 1, fp);

   printf("Loaded chunks = %d\n", chunks);

   if (chunks != 12 || (uintptr_t)saved.PC >= 0x10000 ||
       (uintptr_t)saved.DirectPage > 0x100 || ((uintptr_t)saved.DirectPage & 255) ||
       (uintptr_t)saved.WaitAddress1 > 0x10000 || (uintptr_t)saved.WaitAddress2 > 0x10000) {
      S9xReset(); goto fail;
   }
   IAPU = saved;
   IAPU.PC = IAPU_RAM + (uintptr_t)saved.PC;
   IAPU.DirectPage = IAPU_RAM + (uintptr_t)saved.DirectPage;
   IAPU.WaitAddress1 = saved.WaitAddress1 ? IAPU_RAM + (uintptr_t)saved.WaitAddress1 - 1 : NULL;
   IAPU.WaitAddress2 = saved.WaitAddress2 ? IAPU_RAM + (uintptr_t)saved.WaitAddress2 - 1 : NULL;
   IAPU.RAM = IAPU_RAM;
   CPU.PC = CPU.PCBase = CPU.PCAtOpcodeStart = CPU.WaitAddress = NULL;
   ICPU.UNUSED1 = NULL; ICPU.S9xOpcodes = NULL;
   for (unsigned i = 0; i < NUM_CHANNELS; ++i) {
      SoundData.channels[i].block = SoundData.channels[i].decoded;
      SoundData.channels[i].echo_buf_ptr = NULL;
   }

   FixROMSpeed();
   IPPU.ColorsChanged = true;
   IPPU.OBJChanged = true;
   CPU.InDMA = false;
   S9xFixColourBrightness();
   S9xAPUUnpackStatus();
   S9xFixSoundAfterSnapshotLoad();
   ICPU.ShiftedPB = ICPU.Registers.PB << 16;
   ICPU.ShiftedDB = ICPU.Registers.DB << 16;
   S9xSetPCBase(ICPU.ShiftedPB + ICPU.Registers.PC);
   CPU.PCAtOpcodeStart = CPU.PC;
   S9xUnpackStatus();
   S9xFixCycles();
   S9xReschedule();

   fclose(fp);
   return true;

fail:
   fclose(fp);
   return false;
}
