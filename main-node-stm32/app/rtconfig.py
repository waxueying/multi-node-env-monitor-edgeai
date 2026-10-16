import os

# toolchains options
ARCH = 'arm'
CPU = 'cortex-m4'

# CROSS_TOOL 由环境变量 RTT_CC 决定：
#   gcc  -> scons 用 arm-none-eabi 工具链直接编译（工具链目录走 RTT_EXEC_PATH）
#   keil -> armcc；scons --target=mdk5 会自动置 RTT_CC=keil
CROSS_TOOL = os.getenv('RTT_CC') or 'gcc'

if CROSS_TOOL == 'keil':
    PLATFORM = 'armcc'
    # Keil MDK 的 armcc.exe 所在目录
    EXEC_PATH = 'C:/Keil_v5/ARM/ARMCC/bin'
else:
    PLATFORM = 'gcc'
    EXEC_PATH = os.getenv('RTT_EXEC_PATH') or ''

if PLATFORM == 'gcc':
    PREFIX = 'arm-none-eabi-'
    CC = PREFIX + 'gcc'
    AS = PREFIX + 'gcc'
    AR = PREFIX + 'ar'
    CXX = PREFIX + 'g++'
    LINK = PREFIX + 'gcc'
    TARGET_EXT = 'elf'
    SIZE = PREFIX + 'size'
    OBJDUMP = PREFIX + 'objdump'
    OBJCPY = PREFIX + 'objcopy'
    DEVICE = ' -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard'
    CFLAGS = DEVICE + ' -O0 -g -ffunction-sections -fdata-sections'
    AFLAGS = ' -c' + DEVICE + ' -x assembler-with-cpp -Wa,-mimplicit-it=thumb'
    # 产物统一落在 build/ 下（GCC 的 .o 也在 build/，Keil 的在 build/keil/）
    LFLAGS = DEVICE + ' -Wl,--gc-sections,-Map=build/rt-thread.map,-cref,-u,Reset_Handler -T linkscripts/STM32F407ZG/link.lds'
    CXXFLAGS = DEVICE + ' -O0 -g -ffunction-sections -fdata-sections'
    POST_ACTION = OBJCPY + ' -O binary $TARGET build/rt-thread.bin\n' + SIZE + ' $TARGET\n'
else:
    # Keil MDK / armcc（AC5）
    # 注意：scons --target=mdk5 只生成 .uvprojx，不调用编译器；
    #       LFLAGS/POST_ACTION 留空，避免 GCC 的链接参数被写进 Keil 工程的 Misc 栏。
    #       工程用 Target Dialog 的内存布局（umfTarg=1），不需要 scatter 文件。
    CC = 'armcc'
    AS = 'armasm'
    AR = 'armar'
    CXX = 'armcc'
    LINK = 'armlink'
    TARGET_EXT = 'axf'
    SIZE = 'fromelf --text -z'
    OBJDUMP = 'fromelf -c'
    OBJCPY = 'fromelf --bin'
    DEVICE = ' --cpu Cortex-M4.fp'
    CFLAGS = DEVICE + ' --c99 --gnu -c -g -O0 --apcs=interwork --split_sections'
    AFLAGS = DEVICE + ' -g --apcs=interwork'
    LFLAGS = ''
    CXXFLAGS = CFLAGS
    POST_ACTION = ''

CPATH = ''
LPATH = ''
