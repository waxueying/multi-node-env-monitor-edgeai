#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* 本模块是 C++ 实现（tflite-micro 是 C++ 库），但由 main.c 以 C 方式调用。
 * 没有这段 extern "C"，C++ 编译器会把下面的函数名按 C++ 规则改名，
 * 链接时 main.c 就会报 undefined reference to `ai_infer_init' */
#ifdef __cplusplus
extern "C" {
#endif

/* ===========================================================================
 * AI 推理模块（tflite-micro）—— 把结构安全事件分成三类
 *
 * 模型：1D-CNN，输入 50 个采样点 × 三轴加速度，输出 3 类 softmax
 *   0 = idle    无实质扰动（环境干扰 / 搬货蹭一下）
 *   1 = impact  撞击（一次冲击后快速衰减）
 *   2 = push    持续推压（幅值不大但持续时间长）
 *
 * 与 structure_monitor 的关系：
 *   structure_monitor 是阈值规则，只能回答"有没有超过 0.5g 的撞击"；
 *   本模块用 0.15g 的低阈值触发，再交给模型判断"这一次到底是哪种"，
 *   所以它能识别出规则法看不到的持续推压（幅值多数低于 0.5g）。
 *   两者互不干扰，可同时运行。
 *
 * 触发与对齐（关键，必须与采集固件一致）：
 *   训练数据是"触发点居中"的窗口，模型对事件在窗口里的位置很敏感
 *   （实测偏移超过 ±10 个样本准确率就从 100% 掉到 50%）。
 *   因此本模块不做滑动窗口推理，而是复刻 capture.c 的触发流程：
 *     动态分量 > AI_TRIGGER_G           → 记下触发点
 *     再采 AI_POST_SAMPLES 个样本       → 此刻 100 样本缓冲里触发点正好居中
 *     取中间 50 个样本（扣重力）推理
 *   采样节奏固定 100Hz，每次事件只在结束时推理一次，CPU 占用可忽略。
 *
 * 用法（在 100Hz 的采样任务里）：
 *   ai_infer_init();                      // 初始化一次
 *   ai_infer_feed(ax, ay, az);            // 每次采样调用，单位 g
 *   ai_result_t r;
 *   if (ai_infer_poll(&r)) { ... }        // 有结果时取走
 * =========================================================================== */

/* ---- 类别号（与 train_impact.py 的 CLASS_NAMES 一致） ---- */
#define AI_CLASS_IDLE       0
#define AI_CLASS_IMPACT     1
#define AI_CLASS_PUSH       2

/* 输入窗口参数（固件与训练脚本两侧必须一致，别单独改） */
#define AI_INPUT_SAMPLES    50      /* 送进模型的采样点数（100Hz → 500ms） */
#define AI_TRIGGER_G        0.15f   /* 触发阈值（动态分量，g）：与采集固件同值 */
#define AI_POST_SAMPLES     50      /* 触发后再采多少个样本，使触发点居中 */

/* ---- 一次推理的结果 ---- */
typedef struct
{
    int      cls;       /* 类别号：AI_CLASS_IDLE / IMPACT / PUSH */
    float    conf;      /* 该类的 softmax 置信度，0~1 */
    float    peak_g;    /* 本次窗口内的动态加速度峰值（g） */
    uint32_t seq;       /* 结果序号，从 0 递增 */
} ai_result_t;

/* 初始化：加载模型、分配张量竞技场、校验输入输出形状。
 * 需在上电时调用一次（imu_init() 之后、开始喂数据之前） */
esp_err_t ai_infer_init(void);

/* 送入一帧加速度（单位 g，100Hz 调用一次）。
 * 内部维护重力估计与触发状态机，事件结束时自动完成一次推理 */
void ai_infer_feed(float ax, float ay, float az);

/* 取一条待处理结果。有结果返回 true 并写入 out，没有返回 false */
bool ai_infer_poll(ai_result_t *out);

/* 类别号转名称，越界返回 "?" */
const char *ai_infer_class_name(int cls);

#ifdef __cplusplus
}
#endif
