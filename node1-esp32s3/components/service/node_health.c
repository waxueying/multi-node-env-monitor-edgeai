#include "node_health.h"

/* 连续失败多少次判故障：
 *   IMU 100Hz 采样 → 10 次约 100ms（快一点，撞击监测不能长时间瞎着）
 *   DHT   1Hz 读取 →  3 次约 3s（慢一点，避免偶发干扰误报） */
#define IMU_FAIL_STREAK_THRESH  10
#define DHT_FAIL_STREAK_THRESH  3

typedef struct
{
    uint16_t fail_total;    /* 累计失败次数 */
    uint8_t  fail_streak;   /* 连续失败次数 */
    uint8_t  fault;         /* 1 = 故障中 */
    uint8_t  thresh;        /* 判故障的连续失败阈值 */
} health_t;

static health_t s_imu = { .thresh = IMU_FAIL_STREAK_THRESH };
static health_t s_dht = { .thresh = DHT_FAIL_STREAK_THRESH };

/* 返回 true = 故障状态刚刚变化（刚判故障 或 刚恢复），调用方据此只打印一次 */
static bool health_report(health_t *h, bool ok)
{
    if (ok)
    {
        h->fail_streak = 0;
        if (h->fault)
        {
            h->fault = 0;
            return true;        /* 刚恢复 */
        }
        return false;
    }

    h->fail_total++;
    if (h->fail_streak < 255)
    {
        h->fail_streak++;
    }

    if (!h->fault && h->fail_streak >= h->thresh)
    {
        h->fault = 1;
        return true;            /* 刚判故障 */
    }
    return false;
}

bool node_health_imu_report(bool ok) { return health_report(&s_imu, ok); }
bool node_health_dht_report(bool ok) { return health_report(&s_dht, ok); }

bool     node_health_imu_fault(void)      { return s_imu.fault != 0; }
bool     node_health_dht_fault(void)      { return s_dht.fault != 0; }
uint16_t node_health_imu_fail_total(void) { return s_imu.fail_total; }
uint16_t node_health_dht_fail_total(void) { return s_dht.fail_total; }

