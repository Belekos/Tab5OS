from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parent
names="malloc calloc realloc free memcpy memmove memset memcmp strlen strcmp strncmp strcpy strncpy strchr strrchr strstr strdup fopen fread fwrite fseek ftell fclose feof ferror printf snprintf sprintf vsnprintf puts abort exit qsort rand srand time strncasecmp localtime".split()
args=[sys.executable,str(root.parent/'build.py'),str(root/'app.c'),'--opt','speed','--math','--include',str(root/'compat'),'--include',str(root/'core'),'--include',str(root/'core/libretro/libretro-common/include'),'--include',str(root/'core/libretro'),'--extra-source',str(root.parent/'common/runtime.c'),'--extra-source',str(root.parent/'common/math.c')]
for name in ['main.c','gba_memory.c','savestate.c','input.c','sound.c','cheats.c','memmap.c','serial.c','gbp.c','rfu.c','gba_cc_lut.c','cpu.cpp','video.cpp']:args+=['--extra-source',str(root/'core'/name)]
for name in names:args+=['--define',name+'=tab5_rt_'+name]
for define in ['OVERCLOCK_60FPS','GBA_THUMB_PROFILE=0','GBA_SWI_HLE=2','GBA_P4_THUMB_DYNAREC=0','GBA_XTENSA_DYNAREC=0','ROM_BUFFER_SIZE=7','GBA_SOUND_FREQUENCY=32768']:args+=['--define',define]
subprocess.run(args+sys.argv[1:],check=True)
