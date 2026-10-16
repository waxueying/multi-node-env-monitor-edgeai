#include "ai_infer.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "impact_model_data.h"

/* ---------------------------------------------------------------------------
 * 参数区：下面这 6 个值必须与 main/capture.h、ai_training/train_impact.py
 * 三处保持一致，改动任意一处都要同步，否则模型看到的数据和训练时不是一回事。
 * ------------------------------------------------------------------------- */
#define AI_WINDOW           100     /* 环形缓冲样本数（与 CAP_WINDOW 一致） */
#define AI_SETTLE_N         100     /* 上电多少个样本用于估计重力（1s） */
#define AI_COOLDOWN_N       100     /* 一次推理后冷却多少样本（1s），防止重复触发 */
#define AI_BASELINE_N       20      /* 用缓冲最前面多少样本估重力基线（与训练一致） */
#define AI_GRAV_CALIB_ALPHA 0.05f   /* 上电标定期的重力低通系数（约 0.2s 收敛） */
#define AI_GRAV_LPF_ALPHA   0.01f   /* 正常运行的重力低通系数（约 1s 时间常数） */

/* 张量竞技场：tflite-micro 的中间张量都从这里分配。
 * 本模型只有 16KB 权重、激活张量总共不到 3KB，16KB 留了充足余量；
 * 若 AllocateTensors() 报失败，先把这里调大再排查其它原因。 */
#define AI_ARENA_SIZE       (16 * 1024)

static const char *const s_class_name[3] = { "idle", "impact", "push" };

/* ---- 张量竞技场（16 字节对齐） ---- */
/* alignas 必须放在 static 之前：写成 `static alignas(16) ...` 会被 GCC 拒绝 */
alignas(16) static uint8_t s_arena[AI_ARENA_SIZE];

/* ---- 采样与触发状态（只在调用 ai_infer_feed 的任务里访问，无需加锁） ---- */
static float s_buf[AI_WINDOW][3];   /* 环形缓冲：最近 100 个原始采样（含重力） */
static int   s_idx;                 /* 下一个写入位置，同时是最旧样本位置 */
static int   s_filled;              /* 已写入样本数，用于判断缓冲是否填满 */
static int   s_total;               /* 上电以来总样本数，用于标定计时 */
static float s_grav[3];             /* 重力估计 */
static bool  s_calibrated;          /* 重力估计是否已收敛 */
static int   s_pending;             /* 已触发，正在等导出时机 */
static int   s_countdown;           /* 触发后还差多少个样本可以推理 */
static int   s_cooldown;            /* 推理后冷却还剩多少个样本 */
static float s_peak;                /* 本次窗口内的动态峰值 */
static uint32_t s_seq;              /* 结果序号 */

/* ---- 结果单槽：主任务每 10ms 轮询一次，来得及取走 ---- */
static ai_result_t s_result;
static bool        s_result_ready;

static tflite::MicroInterpreter *s_interp = nullptr;

const char *ai_infer_class_name(int cls)
{
    if (cls < 0 || cls >= 3)
        return "?";
    return s_class_name[cls];
}

/* ---------------------------- 推理 ---------------------------- */
/* 把环形缓冲展开成时间序 → 扣重力基线 → 取中间 50 个样本 → 推理。
 * 调用时缓冲里应当是"触发前 49 个 + 触发点 + 触发后 50 个" */
static void ai_run_inference(void)
{
    if (s_interp == nullptr)
        return;

    /* 1. 展开环形缓冲为时间序（最旧 → 最新） */
    static float raw[AI_WINDOW][3];   /* static：40 字节/样本 ×100 = 1.2KB，
                                       * 放栈上对 4KB 的采样任务太紧张 */
    for (int i = 0; i < AI_WINDOW; i++)
    {
        const float *p = s_buf[(s_idx + i) % AI_WINDOW];
        raw[i][0] = p[0];
        raw[i][1] = p[1];
        raw[i][2] = p[2];
    }

    /* 2. 重力基线：缓冲最前面 AI_BASELINE_N 个样本的均值。
     *    触发点居中，所以这 20 个样本一定在动作之前，是静止段 */
    float base[3] = { 0.0f, 0.0f, 0.0f };
    for (int i = 0; i < AI_BASELINE_N; i++)
    {
        base[0] += raw[i][0];
        base[1] += raw[i][1];
        base[2] += raw[i][2];
    }
    base[0] /= (float)AI_BASELINE_N;
    base[1] /= (float)AI_BASELINE_N;
    base[2] /= (float)AI_BASELINE_N;

    /* 3. 取中间 AI_INPUT_SAMPLES 个样本并扣掉重力，写进模型输入张量。
     *    与 train_impact.py 的 extract_window() 完全一致 */
    const int start = (AI_WINDOW - AI_INPUT_SAMPLES) / 2;
    float *dst = s_interp->input(0)->data.f;
    for (int i = 0; i < AI_INPUT_SAMPLES; i++)
    {
        dst[i * 3 + 0] = raw[start + i][0] - base[0];
        dst[i * 3 + 1] = raw[start + i][1] - base[1];
        dst[i * 3 + 2] = raw[start + i][2] - base[2];
    }

    /* 4. 推理 */
    if (s_interp->Invoke() != kTfLiteOk)
    {
        /* printf("[AI] 推理失败\n"); */
        return;
    }

    /* 5. 取最大值那一类 */
    const float *out = s_interp->output(0)->data.f;
    int best = 0;
    for (int i = 1; i < 3; i++)
    {
        if (out[i] > out[best])
            best = i;
    }

    s_result.cls    = best;
    s_result.conf   = out[best];
    s_result.peak_g = s_peak;
    s_result.seq    = s_seq++;
    s_result_ready  = true;
}

esp_err_t ai_infer_init(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    memset(s_grav, 0, sizeof(s_grav));
    s_idx = s_filled = s_total = 0;
    s_calibrated = false;
    s_pending = s_countdown = s_cooldown = 0;
    s_peak = 0.0f;
    s_seq = 0;
    s_result_ready = false;
    memset(&s_result, 0, sizeof(s_result));

    /* 1. 校验模型 */
    const tflite::Model *model = tflite::GetModel(impact_model_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        /* printf("[AI] 模型版本不匹配：模型 %d，运行时 %d\n",
               (int)model->version(), (int)TFLITE_SCHEMA_VERSION); */
        return ESP_ERR_INVALID_VERSION;
    }
    /* printf("[AI] 模型 %u 字节，张量竞技场 %u 字节\n",
           (unsigned)impact_model_tflite_len, (unsigned)AI_ARENA_SIZE); */

    /* 2. 注册本模型用到的算子。
     *    模型是 Conv1D + MaxPool1D + Dense，转成 tflite 后对应
     *    CONV_2D / MAX_POOL_2D / FULLY_CONNECTED，量化后另有
     *    QUANTIZE / DEQUANTIZE，Conv1D 转换过程会插入 RESHAPE 与 EXPAND_DIMS。
     *    注册多了不影响运行，但别注册少了 */
    static tflite::MicroMutableOpResolver<10> resolver;
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddFullyConnected();
    resolver.AddReshape();
    resolver.AddSoftmax();
    resolver.AddQuantize();
    resolver.AddDequantize();
    resolver.AddExpandDims();

    /* 3. 建解释器并分配张量 */
    static tflite::MicroInterpreter interp(model, resolver, s_arena, sizeof(s_arena));
    s_interp = &interp;

    if (interp.AllocateTensors() != kTfLiteOk)
    {
        /* printf("[AI] 张量分配失败，请把 AI_ARENA_SIZE 调大\n"); */
        s_interp = nullptr;
        return ESP_FAIL;
    }

    /* 4. 校验输入输出形状，防止模型和固件参数对不上 */
    TfLiteTensor *in  = interp.input(0);
    TfLiteTensor *out = interp.output(0);
    const bool shape_ok = (in->dims->size == 3 &&
                           in->dims->data[1] == AI_INPUT_SAMPLES &&
                           in->dims->data[2] == 3 &&
                           in->type == kTfLiteFloat32 &&
                           out->dims->size == 2 &&
                           out->dims->data[1] == 3 &&
                           out->type == kTfLiteFloat32);
    if (!shape_ok)
    {
        /* printf("[AI] 模型输入输出与固件约定不符，请检查模型是否换过\n"); */
        s_interp = nullptr;
        return ESP_ERR_INVALID_ARG;
    }

    /* printf("[AI] 就绪：输入 %d 样本 x 3 轴，输出 [idle, impact, push]，"
           "张量区占用 %u/%u 字节\n",
           AI_INPUT_SAMPLES,
           (unsigned)interp.arena_used_bytes(), (unsigned)AI_ARENA_SIZE); */
    return ESP_OK;
}

void ai_infer_feed(float ax, float ay, float az)
{
    /* 1. 无条件写入环形缓冲（存原始值，去重力在推理时做） */
    s_buf[s_idx][0] = ax;
    s_buf[s_idx][1] = ay;
    s_buf[s_idx][2] = az;
    s_idx = (s_idx + 1) % AI_WINDOW;
    if (s_filled < AI_WINDOW)
        s_filled++;
    s_total++;

    /* 2. 动态分量 = 实测 - 重力估计，静止时约 0 */
    const float dx = ax - s_grav[0];
    const float dy = ay - s_grav[1];
    const float dz = az - s_grav[2];
    const float dyn = sqrtf(dx * dx + dy * dy + dz * dz);

    /* 3. 上电标定：这段时间只学重力，不触发 */
    if (!s_calibrated)
    {
        s_grav[0] += (ax - s_grav[0]) * AI_GRAV_CALIB_ALPHA;
        s_grav[1] += (ay - s_grav[1]) * AI_GRAV_CALIB_ALPHA;
        s_grav[2] += (az - s_grav[2]) * AI_GRAV_CALIB_ALPHA;
        if (s_total >= AI_SETTLE_N)
            s_calibrated = true;
        return;
    }

    /* 4. 只在准静态时更新重力估计，避免把撞击/推压当成新的重力 */
    if (dyn < AI_TRIGGER_G)
    {
        s_grav[0] += (ax - s_grav[0]) * AI_GRAV_LPF_ALPHA;
        s_grav[1] += (ay - s_grav[1]) * AI_GRAV_LPF_ALPHA;
        s_grav[2] += (az - s_grav[2]) * AI_GRAV_LPF_ALPHA;
    }

    /* 5. 冷却期内不触发，避免一个动作算好几次 */
    if (s_cooldown > 0)
    {
        s_cooldown--;
        return;
    }

    /* 6. 已触发：再采满 AI_POST_SAMPLES 个样本，此时触发点正好落在缓冲区中央 */
    if (s_pending)
    {
        if (dyn > s_peak)
            s_peak = dyn;

        if (--s_countdown <= 0)
        {
            s_pending  = 0;
            s_cooldown = AI_COOLDOWN_N;
            ai_run_inference();
        }
        return;
    }

    /* 7. 触发判定：缓冲区已填满且动态分量越过阈值 */
    if (s_filled >= AI_WINDOW && dyn > AI_TRIGGER_G)
    {
        s_pending   = 1;
        s_countdown = AI_POST_SAMPLES;
        s_peak      = dyn;
    }
}

bool ai_infer_poll(ai_result_t *out)
{
    if (!s_result_ready)
        return false;

    s_result_ready = false;
    if (out != nullptr)
        *out = s_result;
    return true;
}
