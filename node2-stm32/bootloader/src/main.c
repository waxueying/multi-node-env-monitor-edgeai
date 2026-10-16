#include <stdint.h>

/* ============================================================================
 * 主节点 Bootloader（STM32F407ZGT6）
 *
 * 职责（"真双区"方案 —— 只决定从哪个区启动，不做搬运）：
 *     上电 → 读标志区 → 决定目标区 → 校验镜像 → 设 MSP/VTOR → 跳转
 *
 * Flash 布局（1MB）：
 *     0x08000000   48KB   本 bootloader     (Sector 0~2)
 *     0x0800C000   16KB   升级标志区         (Sector 3)
 *     0x08010000  448KB   APP-A（默认运行区） (Sector 4~7)
 *     0x08080000  512KB   APP-B             (Sector 8~11)
 *
 * 设计原则：**不用 HAL、不用 RTOS、不初始化任何外设**。
 *     越简单越不容易出错；时钟保持复位默认的 HSI 16MHz 就够跳转了，
 *     真正的外设初始化交给 app 去做。
 *
 * 切换逻辑（"pending 只消费一次"，防变砖）：
 *   有 pending 且目标区镜像合法 → 清 pending → 跳过去试用（app 起来后自确认）；
 *   有 pending 但目标区镜像非法 → 清 pending → 跳回 active（坏包 / 空区自动回滚）；
 *   没有 pending                → 直接跳 active。
 *   新固件若没来得及确认就崩了，下次复位时 pending 已清零、active 仍是旧区，
 *   于是自动回到旧固件 —— 这就是"消费一次"的回滚原理。
 * ========================================================================== */

/* ---- 地址 / 标志 ---- */
#define FLAG_ADDR       0x08010000UL
#define APP_A_ADDR      0x08020000UL
#define APP_B_ADDR      0x08040000UL

#define FLAG_MAGIC      0x5A5AA5A5UL
#define SLOT_A          0x41UL          /* 'A' */
#define SLOT_B          0x42UL          /* 'B' */

/* ---- 几个寄存器：不引 CMSIS，手写就够（省掉一堆头文件和 include 路径）---- */
#define REG32(a)            (*(volatile uint32_t *)(a))
#define SCB_VTOR            REG32(0xE000ED08UL)     /* 向量表基址 */
#define ICSR                REG32(0xE000ED04UL)     /* 中断控制状态 */
#define SYSTICK_CTRL        REG32(0xE000E010UL)
#define SYSTICK_LOAD        REG32(0xE000E014UL)
#define SYSTICK_VAL         REG32(0xE000E018UL)

/* ---- FLASH 寄存器（F4 的 FLASH 控制器在 0x40023C00；0xA0000000 是 FSMC，别写错）---- */
#define FLASH_KEYR          REG32(0x40023C04UL)
#define FLASH_SR            REG32(0x40023C0CUL)
#define FLASH_CR            REG32(0x40023C10UL)

#define FLASH_KEY1          0x45670123UL
#define FLASH_KEY2          0xCDEF89ABUL
#define FLASH_SR_BSY        0x00010000UL        /* 忙（bit16） */
#define FLASH_SR_ERR        0x000000F2UL        /* 各类错误标志（写 1 清） */
#define FLASH_SR_EOP        0x00000001UL        /* 操作完成标志 */
#define FLASH_CR_PG         0x00000001UL        /* 编程 */
#define FLASH_CR_SER        0x00000002UL        /* 扇区擦除 */
#define FLASH_CR_MER        0x00000004UL        /* 全片擦除（清理 CR 时一并清掉） */
#define FLASH_CR_SNB_POS    3                   /* SNB[6:3] */
#define FLASH_CR_STRT       0x00010000UL        /* 开始擦除 */
#define FLASH_CR_LOCK       0x80000000UL        /* 上锁 */

#define FLASH_CR_PSIZE      0x00000300UL        /* 编程并行度位（bit9:8） */
#define FLASH_CR_PSIZE_X32  0x00000200UL        /* x32：对应 VDD 2.7~3.6V */

#define FLAG_SECTOR         4                   /* 标志区 = Sector 4（0x08010000，64KB） */

/* 标志区记录：12 字节数据 + 1 个字校验（异或），放在 0x0800C000 开头 */
typedef struct
{
    uint32_t magic;         /* FLAG_MAGIC 才认为记录有效 */
    uint32_t active;        /* 当前应运行的区：SLOT_A / SLOT_B */
    uint32_t pending;       /* 待验证的目标区：0 = 没有 */
    uint32_t check;         /* 前三个字的异或（简单校验，够用） */
} boot_flag_t;

/* ---- 关总中断 + 设 MSP + 跳转（内联汇编，一步到位、不返回）---- */
/* 注意：设了 MSP 之后本函数的返回地址就靠不住了，所以跳转必须在汇编里完成 */
__asm void boot_jump(uint32_t sp, uint32_t pc)
{
    CPSID   i
    MSR     MSP, r0
    BX      r1
}

/* 使能 FPU：把 CPACR 里 CP10/CP11 设成全访问，再用 DSB/ISB 让设置立即生效。
 * （汇编块里不能出现非 ASCII 字符，所以说明写在外面） */
__asm void boot_enable_fpu(void)
{
    LDR     r0, =0xE000ED88
    LDR     r1, [r0]
    ORR     r1, r1, #0xF00000
    STR     r1, [r0]
    DSB
    ISB
    BX      lr
}

/* 关总中断（CPSID i） */
__asm void boot_disable_irq(void)
{
    CPSID   i
    BX      lr
}

/* startup 文件会调用 SystemInit()；bootloader 不需要配时钟（用默认 HSI 就够），
 * 但**必须在这里使能 FPU** —— 这是一条很容易踩的坑：
 *
 *   Cortex-M4F 复位后 FPU 是关的（CPACR 里 CP10/CP11 禁止访问），而 ARM 的库函数
 *   （scatter loading、memcpy/memset 这些）在"硬件浮点"编译选项下会用 VFP 指令，
 *   一旦访问未使能的协处理器就会触发 NOCP UsageFault → 直接进 HardFault。
 *
 *   所以哪怕 bootloader 自己一行浮点代码都没有，这个使能也省不掉。 */
void SystemInit(void)
{
    boot_enable_fpu();

    /* 主动关掉 SysTick 和总中断 —— **不能假设复位后状态是干净的**：
     *   1) 调试器用 VECTRESET / 不复位时，SCS 里的 SysTick 使能位不会清掉，
     *      上次运行的固件开着 SysTick，bootloader 一进来就被中断打进来；
     *   2) 而 startup 里 SysTick_Handler 是空壳（B .），一进去就卡死。
     * bootloader 期间本来就不需要任何中断，索性全部关掉。 */
    SYSTICK_CTRL = 0;
    SYSTICK_LOAD = 0;
    SYSTICK_VAL  = 0;
    boot_disable_irq();
}

/* 读标志区；校验不过就当"没有记录" */
static void read_flag(boot_flag_t *f)
{
    const boot_flag_t *p = (const boot_flag_t *)FLAG_ADDR;

    *f = *p;
    if (f->check != (f->magic ^ f->active ^ f->pending))
    {
        f->magic = 0;
    }
}

/* 这个区里放的是不是一个"能启动的镜像"？
 * Cortex-M 的向量表前两个字是：初始 MSP、复位向量 —— 检查它们落不落在合法地址 */
static int image_valid(uint32_t addr)
{
    uint32_t sp = REG32(addr);
    uint32_t pc = REG32(addr + 4);

    if (sp < 0x20000000UL || sp > 0x20020000UL) return 0;    /* 栈顶要在 SRAM 128KB 内 */
    if (pc < 0x08000000UL || pc > 0x08080000UL) return 0;    /* 入口要在 flash 512KB 内 */
    if ((pc & 1UL) == 0) return 0;                           /* Thumb 位必须为 1 */
    return 1;
}

/* 等 Flash 操作完成；返回 0 = 成功，-1 = 出错/超时（错误标志已清） */
static int flash_wait(void)
{
    uint32_t i;

    /* 有上限的等待：万一控制器卡住，不能把 bootloader 挂死 */
    for (i = 0; (FLASH_SR & FLASH_SR_BSY) && (i < 20000000UL); i++)
    {
    }

    if (FLASH_SR & (FLASH_SR_ERR | FLASH_SR_BSY))
    {
        FLASH_SR = FLASH_SR_ERR;            /* 写 1 清错误标志 */
        return -1;
    }
    return 0;
}

/* 擦除 + 写入标志区（只擦 Sector 3，不碰 bootloader 自己所在的 0~2）。
 * 写入前重算 check，调用方只改 magic/active/pending 字段即可 */
static int write_flag(boot_flag_t *f)
{
    const uint32_t *src;
    uint32_t i;
    int ret;

    f->check = f->magic ^ f->active ^ f->pending;
    src = (const uint32_t *)f;

    /* 解锁 + 清理残留状态。不能假设复位后是干净的：系统复位不清 FLASH_SR
     * （Keil 烧录 / 上次失败会留下错误标志），CR 里也可能有残留位 */
    if (FLASH_CR & FLASH_CR_LOCK)
    {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }

    for (i = 0; (FLASH_SR & FLASH_SR_BSY) && (i < 20000000UL); i++)
    {
    }

    FLASH_SR = FLASH_SR_ERR | FLASH_SR_EOP;     /* 写 1 清残留标志 */

    FLASH_CR &= ~(FLASH_CR_PG | FLASH_CR_SER | FLASH_CR_MER | FLASH_CR_PSIZE |
                  (0xFUL << FLASH_CR_SNB_POS));
    FLASH_CR |= FLASH_CR_PSIZE_X32;

    /* 擦除 Sector 3（先设并行度 x32，和 HAL 的做法一致） */
    FLASH_CR &= ~FLASH_CR_PSIZE;
    FLASH_CR |= FLASH_CR_PSIZE_X32;

    FLASH_CR &= ~(0xFUL << FLASH_CR_SNB_POS);
    FLASH_CR |= FLASH_CR_SER | (FLAG_SECTOR << FLASH_CR_SNB_POS);
    FLASH_CR |= FLASH_CR_STRT;
    ret = flash_wait();
    FLASH_CR &= ~FLASH_CR_SER;

    /* 按字写入（16 字节记录 = 4 个字） */
    for (i = 0; ret == 0 && i < 4; i++)
    {
        if (flash_wait() != 0)
        {
            ret = -1;
            break;
        }

        FLASH_CR &= ~FLASH_CR_PSIZE;
        FLASH_CR |= FLASH_CR_PSIZE_X32;
        FLASH_CR |= FLASH_CR_PG;
        REG32(FLAG_ADDR + i * 4UL) = src[i];

        if (flash_wait() != 0)
        {
            ret = -1;
        }
        FLASH_CR &= ~FLASH_CR_PG;
    }

    /* 上锁 */
    FLASH_CR |= FLASH_CR_LOCK;
    return ret;
}

/* 决定目标区 → 校验 → 跳 */
static void boot_to(uint32_t addr)
{
    uint32_t sp = REG32(addr);
    uint32_t pc = REG32(addr + 4);

    SYSTICK_CTRL = 0;                       /* 关掉 SysTick，别把 bootloader 的节拍带过去 */
    SYSTICK_LOAD = 0;
    SYSTICK_VAL  = 0;
    ICSR         = 0x07FFFFFFUL;            /* 清掉所有可清的中断挂起位 */

    SCB_VTOR = addr;                        /* 换向量表：app 的中断从此走自己的表 */

    boot_jump(sp, pc);                      /* 关中断 + 设 MSP + 跳，不返回 */
}

int main(void)
{
    boot_flag_t f;
    uint32_t target;

    read_flag(&f);

    /* 决定跳哪个区：
     *   没有有效标志（首次上电 / 标志被擦过）→ 默认 A 区
     *   有 pending（刚升级完，等验证）        → 校验目标区：合法跳过去、坏包回滚
     *   否则                                  → 跳 active 记录的区 */
    if (f.magic != FLAG_MAGIC)
    {
        target = APP_A_ADDR;
    }
    else if (f.pending != 0)
    {
        uint32_t slot_addr = (f.pending == SLOT_B) ? APP_B_ADDR : APP_A_ADDR;

        if (image_valid(slot_addr))
        {
            /* 目标区镜像合法：消费掉 pending（只消费一次），跳过去试用 */
            f.pending = 0;
            write_flag(&f);
            target = slot_addr;
        }
        else
        {
            /* 目标区镜像非法（坏包 / 空区）：清 pending，回滚到 active */
            f.pending = 0;
            write_flag(&f);
            target = (f.active == SLOT_B) ? APP_B_ADDR : APP_A_ADDR;
        }
    }
    else
    {
        target = (f.active == SLOT_B) ? APP_B_ADDR : APP_A_ADDR;
    }

    /* 目标区不合法（空白 / 被擦坏）就换另一个区兜底 */
    if (!image_valid(target))
    {
        target = (target == APP_A_ADDR) ? APP_B_ADDR : APP_A_ADDR;
    }

    /* 两个区都没镜像：停在 bootloader（真板子上靠调试器看，没有 LED）*/
    if (!image_valid(target))
    {
        while (1)
        {
        }
    }

    boot_to(target);
    return 0;                               /* 不会执行到这里 */
}

