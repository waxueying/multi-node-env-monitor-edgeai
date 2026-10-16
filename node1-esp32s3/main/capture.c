#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "capture.h"

#if CAPTURE_MODE

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp_imu.h"

/* 串口命令轮询周期：20ms 足够跟上人的手速，又不至于空转占满 CPU */
#define CAP_CMD_POLL_MS     20

/* 重力估计低通系数：100Hz 下约 1s 时间常数，与 structure_monitor 取同一量级 */
#define CAP_GRAV_LPF_ALPHA  0.01f

/* 上电标定阶段用较快的系数（约 0.2s 收敛） */
#define CAP_GRAV_CALIB_ALPHA 0.05f

/* 一次导出的文本长度上限：100 行 × 最长约 25 字节 + 帧头，留足余量 */
#define CAP_DUMP_BUF_SIZE   4096

static const char *const s_class_name[4] = {
    "idle 干扰（走路/搬货）",
    "tap  轻拍（快速衰减）",
    "push 推压（持续时间长）",
    "hit  冲击（剧烈、多次振荡）",
};

/* ---- 采样状态：只被采样任务访问，无需加锁 ---- */
static float s_buf[CAP_WINDOW][3];   /* 环形缓冲：原始三轴加速度（单位 g，含重力） */
static int   s_idx;                  /* 下一个写入位置，同时也是"最旧样本"的位置 */
static int   s_filled;               /* 已写入样本数，用于判断缓冲区是否填满 */
static int   s_total;                /* 上电以来总样本数，用于标定计时 */
static float s_grav[3];              /* 重力估计（动态分量 = 实测 - 重力估计） */
static bool  s_calibrated;           /* 重力估计是否已收敛 */
static int   s_pending;              /* 已触发，正在等导出时机 */
static int   s_countdown;            /* 触发后还差多少个样本导出 */
static int   s_cooldown;             /* 导出后冷却还剩多少个样本 */
static uint32_t s_seq;               /* 本次上电的导出序号 */

/* ---- 当前类别：命令任务写、采样任务读，故用 volatile ---- */
static volatile int s_class = CAP_START_CLASS;

/* ---- 手动导出请求：命令任务置 1，采样任务在下一个采样点消费 ---- */
static volatile int s_force_dump = 0;

/* ============================ 串口输出 ============================ */

/* 把整段数据拼进一个缓冲区后一次性写出。
 * 这样它只是一次写操作，不会和命令任务的提示文字交错到数据集里面。 */
static void cap_dump(void)
{
    static char buf[CAP_DUMP_BUF_SIZE];
    char line[48];
    int  cls = s_class;
    int  n   = 0;

    n += snprintf(buf + n, sizeof(buf) - n, "#CAP,%d,%u\n", cls, (unsigned)s_seq++);

    /* 从最旧到最新输出 */
    for (int i = 0; i < CAP_WINDOW; i++)
    {
        const float *p = s_buf[(s_idx + i) % CAP_WINDOW];
        int m = snprintf(line, sizeof(line), "%.4f,%.4f,%.4f\n", p[0], p[1], p[2]);
        if (m < 0)
            continue;
        if (m > (int)sizeof(line) - 1)      /* 数值异常变大时截断该行，不越界 */
            m = sizeof(line) - 1;
        if (n + m >= (int)sizeof(buf))
            break;
        memcpy(buf + n, line, m);
        n += m;
    }

    if (n + 1 < (int)sizeof(buf))
        buf[n++] = '\n';                    /* 空行 = 一段数据的结束标记 */

    fwrite(buf, 1, n, stdout);
}

static void cap_print_help(void)
{
    printf("\n");
    printf("=============== 采集模式 ===============\n");
    printf("  发送 0/1/2/3 切换类别（不用按回车）\n");
    printf("    0 = idle 干扰（没人碰货架）\n");
    printf("    1 = tap  轻拍\n");
    printf("    2 = push 持续推压\n");
    printf("    3 = hit  剧烈冲击\n");
    printf("  发送 e 立刻导出一次（0 = idle 静止数据只能这样采）\n");
    printf("  发送 h 重新显示本说明\n");
    printf("  1/2/3 类动作会触发自动导出 100 行数据（触发点居中）\n");
    printf("=======================================\n");
}

/* ============================ 采样与触发 ============================ */

/* 送入一帧采样并推进状态机。
 * 返回 true 表示本次调用完成了一次导出（调用方据此重置节拍基准）。 */
static bool cap_feed(float ax, float ay, float az)
{
    /* 1. 无条件写入环形缓冲（存原始值，去重力交给训练脚本做） */
    s_buf[s_idx][0] = ax;
    s_buf[s_idx][1] = ay;
    s_buf[s_idx][2] = az;
    s_idx = (s_idx + 1) % CAP_WINDOW;
    if (s_filled < CAP_WINDOW)
        s_filled++;
    s_total++;

    /* 2. 动态分量 = 实测加速度 - 重力估计，静止时约为 0 */
    float dx = ax - s_grav[0];
    float dy = ay - s_grav[1];
    float dz = az - s_grav[2];
    float dyn = sqrtf(dx * dx + dy * dy + dz * dz);

    /* 3. 上电标定：这段时间内只学重力，不触发 */
    if (!s_calibrated)
    {
        s_grav[0] += (ax - s_grav[0]) * CAP_GRAV_CALIB_ALPHA;
        s_grav[1] += (ay - s_grav[1]) * CAP_GRAV_CALIB_ALPHA;
        s_grav[2] += (az - s_grav[2]) * CAP_GRAV_CALIB_ALPHA;
        if (s_total >= CAP_SETTLE_N)
        {
            s_calibrated = true;
            printf("[采集] 重力标定完成（g = %.3f %.3f %.3f），当前类别 %d\n",
                   s_grav[0], s_grav[1], s_grav[2], (int)s_class);
        }
        return false;
    }

    /* 4. 只在准静态时更新重力估计，否则会把撞击/推压当成新的重力 */
    if (dyn < CAP_TRIGGER_G)
    {
        s_grav[0] += (ax - s_grav[0]) * CAP_GRAV_LPF_ALPHA;
        s_grav[1] += (ay - s_grav[1]) * CAP_GRAV_LPF_ALPHA;
        s_grav[2] += (az - s_grav[2]) * CAP_GRAV_LPF_ALPHA;
    }

    /* 5. 手动导出：idle 类没有触发点，只能靠串口发 e 采。
     *    在采样点里执行（而不是在命令任务里），保证缓冲内容不带竞态。 */
    if (s_force_dump)
    {
        s_force_dump = 0;
        if (s_filled >= CAP_WINDOW)
        {
            s_pending  = 0;
            s_cooldown = CAP_COOLDOWN_N;
            cap_dump();
            return true;
        }
    }

    /* 6. 冷却期内不触发，避免一个动作导出多份 */
    if (s_cooldown > 0)
    {
        s_cooldown--;
        return false;
    }

    /* 7. 已触发：再采满 CAP_POST_SAMPLES 个样本，此时缓冲里正好是
     *    "触发前 49 个 + 触发点 + 触发后 50 个"，可以导出了 */
    if (s_pending)
    {
        if (--s_countdown <= 0)
        {
            s_pending  = 0;
            s_cooldown = CAP_COOLDOWN_N;
            cap_dump();
            return true;
        }
        return false;
    }

    /* 8. 触发判定：缓冲已填满，且动态分量越过阈值 */
    if (s_filled >= CAP_WINDOW && dyn > CAP_TRIGGER_G)
    {
        s_pending   = 1;
        s_countdown = CAP_POST_SAMPLES;
    }
    return false;
}

/* ============================== 任务 ============================== */

static void capture_sample_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();

    while (1)
    {
        float ax, ay, az;

        if (imu_read_accel(&ax, &ay, &az) == ESP_OK)
        {
            if (cap_feed(ax, ay, az))
            {
                /* 导出 100 行会阻塞约 0.25s。重置节拍基准，
                 * 否则 vTaskDelayUntil 会立刻连补几十个样本、打乱时间轴 */
                last_wake = xTaskGetTickCount();
            }
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CAP_SAMPLE_MS));
    }
}

static void capture_console_task(void *arg)
{
    char c;

    while (1)
    {
        /* 控制台 VFS 默认非阻塞：没数据时 read 返回 -1，有数据才返回 1 */
        if (read(STDIN_FILENO, &c, 1) == 1)
        {
            if (c >= '0' && c <= '3')
            {
                s_class = c - '0';
                printf("[采集] 当前类别 -> %d %s\n", (int)s_class, s_class_name[s_class]);
            }
            else if (c == 'h' || c == 'H' || c == '?')
            {
                cap_print_help();
            }
            else if (c == 'e' || c == 'E')
            {
                s_force_dump = 1;
                printf("[采集] 手动导出，类别 %d %s\n", (int)s_class, s_class_name[s_class]);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(CAP_CMD_POLL_MS));
    }
}

void capture_mode_run(void)
{
    ESP_ERROR_CHECK(imu_init());

    printf("\n[采集] 节点1 进入数据采集模式：%d Hz，每次导出 %d 个样本\n",
           1000 / CAP_SAMPLE_MS, CAP_WINDOW);
    cap_print_help();

    /* 分核思路沿用正常运行固件：命令任务在核心0（低优先级），采样任务独占核心1 */
    xTaskCreatePinnedToCore(capture_console_task, "cap_cmd", 3072, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(capture_sample_task,  "cap_smp", 4096, NULL, 6, NULL, 1);
}

#endif /* CAPTURE_MODE */
