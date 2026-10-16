/**
  ******************************************************************************
  * @file    network1.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2025-12-02T14:52:04+0800
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */


#include "network1.h"
#include "network1_data.h"

#include "ai_platform.h"
#include "ai_platform_interface.h"
#include "ai_math_helpers.h"

#include "core_common.h"
#include "core_convert.h"

#include "layers.h"



#undef AI_NET_OBJ_INSTANCE
#define AI_NET_OBJ_INSTANCE g_network1
 
#undef AI_NETWORK1_MODEL_SIGNATURE
#define AI_NETWORK1_MODEL_SIGNATURE     "0xa8bf6cd41eba4b36effd7962eb76a1bd"

#ifndef AI_TOOLS_REVISION_ID
#define AI_TOOLS_REVISION_ID     ""
#endif

#undef AI_TOOLS_DATE_TIME
#define AI_TOOLS_DATE_TIME   "2025-12-02T14:52:04+0800"

#undef AI_TOOLS_COMPILE_TIME
#define AI_TOOLS_COMPILE_TIME    __DATE__ " " __TIME__

#undef AI_NETWORK1_N_BATCHES
#define AI_NETWORK1_N_BATCHES         (1)

static ai_ptr g_network1_activations_map[1] = AI_C_ARRAY_INIT;
static ai_ptr g_network1_weights_map[1] = AI_C_ARRAY_INIT;



/**  Array declarations section  **********************************************/
/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  serving_default_keras_tensor0_output_array, AI_ARRAY_FORMAT_FLOAT|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 3, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  conversion_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  gemm_1_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 64, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  gemm_2_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 32, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  gemm_3_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 16, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  gemm_4_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 32, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  gemm_5_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 64, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  gemm_6_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  conversion_7_output_array, AI_ARRAY_FORMAT_FLOAT|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 3, AI_STATIC)

/* Array#9 */
AI_ARRAY_OBJ_DECLARE(
  gemm_1_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 192, AI_STATIC)

/* Array#10 */
AI_ARRAY_OBJ_DECLARE(
  gemm_1_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 64, AI_STATIC)

/* Array#11 */
AI_ARRAY_OBJ_DECLARE(
  gemm_2_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2048, AI_STATIC)

/* Array#12 */
AI_ARRAY_OBJ_DECLARE(
  gemm_2_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 32, AI_STATIC)

/* Array#13 */
AI_ARRAY_OBJ_DECLARE(
  gemm_3_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 512, AI_STATIC)

/* Array#14 */
AI_ARRAY_OBJ_DECLARE(
  gemm_3_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 16, AI_STATIC)

/* Array#15 */
AI_ARRAY_OBJ_DECLARE(
  gemm_4_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 512, AI_STATIC)

/* Array#16 */
AI_ARRAY_OBJ_DECLARE(
  gemm_4_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 32, AI_STATIC)

/* Array#17 */
AI_ARRAY_OBJ_DECLARE(
  gemm_5_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2048, AI_STATIC)

/* Array#18 */
AI_ARRAY_OBJ_DECLARE(
  gemm_5_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 64, AI_STATIC)

/* Array#19 */
AI_ARRAY_OBJ_DECLARE(
  gemm_6_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 192, AI_STATIC)

/* Array#20 */
AI_ARRAY_OBJ_DECLARE(
  gemm_6_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 3, AI_STATIC)

/* Array#21 */
AI_ARRAY_OBJ_DECLARE(
  gemm_1_scratch0_array, AI_ARRAY_FORMAT_S16,
  NULL, NULL, 323, AI_STATIC)

/* Array#22 */
AI_ARRAY_OBJ_DECLARE(
  gemm_2_scratch0_array, AI_ARRAY_FORMAT_S16,
  NULL, NULL, 224, AI_STATIC)

/* Array#23 */
AI_ARRAY_OBJ_DECLARE(
  gemm_3_scratch0_array, AI_ARRAY_FORMAT_S16,
  NULL, NULL, 112, AI_STATIC)

/* Array#24 */
AI_ARRAY_OBJ_DECLARE(
  gemm_4_scratch0_array, AI_ARRAY_FORMAT_S16,
  NULL, NULL, 176, AI_STATIC)

/* Array#25 */
AI_ARRAY_OBJ_DECLARE(
  gemm_5_scratch0_array, AI_ARRAY_FORMAT_S16,
  NULL, NULL, 352, AI_STATIC)

/* Array#26 */
AI_ARRAY_OBJ_DECLARE(
  gemm_6_scratch0_array, AI_ARRAY_FORMAT_S16,
  NULL, NULL, 79, AI_STATIC)

/**  Array metadata declarations section  *************************************/
/* Int quant #0 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(conversion_0_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(3.9863665103912354f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_1_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.2703133821487427f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_1_weights_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 64,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0011081815464422107f, 0.0021247095428407192f, 0.0020979312248528004f, 0.001876885537058115f, 0.0019776704721152782f, 0.0010099587962031364f, 0.002292053773999214f, 0.002364051528275013f, 0.002213272964581847f, 0.0016081485664471984f, 0.0015717007918283343f, 0.001813837792724371f, 0.0006309971795417368f, 0.002211774466559291f, 0.0021260797511786222f, 0.0005660249735228717f, 0.000981700955890119f, 0.002450682921335101f, 0.0025227623991668224f, 0.0023428103886544704f, 0.002121465979143977f, 0.002045723143965006f, 0.0013199993409216404f, 0.002203756244853139f, 0.0019655402284115553f, 0.0022777190897613764f, 0.00197281944565475f, 0.0020972650963813066f, 0.0013156285276636481f, 0.0024714204482734203f, 0.0011281443294137716f, 0.0020657905843108892f, 0.0017939613899216056f, 0.001841531484387815f, 0.0015833878424018621f, 0.0023355663288384676f, 0.0009816632373258471f, 0.002234106417745352f, 0.0012911142548546195f, 0.002307788236066699f, 0.0018840227276086807f, 0.0012661742512136698f, 0.0025402531027793884f, 0.0020938944071531296f, 0.0021848673932254314f, 0.0021912031807005405f, 0.0014360849745571613f, 0.0019477527821436524f, 0.001729222945868969f, 0.0013659883989021182f, 0.0018654875457286835f, 0.0017175920074805617f, 0.002036414807662368f, 0.0016207018634304404f, 0.00019922570209018886f, 0.0022169770672917366f, 0.0016020762268453836f, 0.0018917940324172378f, 0.0013235638616606593f, 0.002133011817932129f, 0.00188946642447263f, 0.0017618837300688028f, 0.0017623747698962688f, 0.0019911883864551783f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_2_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(2.2264692783355713f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_2_weights_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 32,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0021724530961364508f, 0.002123854588717222f, 0.0018372905906289816f, 0.0019364708568900824f, 0.0019286369206383824f, 0.0020154567901045084f, 0.0020478020887821913f, 0.0020103806164115667f, 0.001957830972969532f, 0.0019595681224018335f, 0.0019361695740371943f, 0.0019443180644884706f, 0.0019469988765195012f, 0.001870403066277504f, 0.0021821174304932356f, 0.0019679900724440813f, 0.0021971326787024736f, 0.0019886575173586607f, 0.0019281919812783599f, 0.0019620731472969055f, 0.0019365370972082019f, 0.0019451872212812304f, 0.0018372907070443034f, 0.0021576681174337864f, 0.0019522272050380707f, 0.0019138670759275556f, 0.0019402268808335066f, 0.001931629260070622f, 0.0019492215942591429f, 0.0019318503327667713f, 0.0019579699728637934f, 0.0019476014422252774f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_3_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(2.899355888366699f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_3_weights_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 16,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.002715394599363208f, 0.002638571197167039f, 0.0026640186551958323f, 0.0029827095568180084f, 0.0027556074783205986f, 0.002970178844407201f, 0.0026025145780295134f, 0.0028853544499725103f, 0.002777405548840761f, 0.002859698375687003f, 0.0027576216962188482f, 0.002724295249208808f, 0.0025559791829437017f, 0.002898616250604391f, 0.002774920780211687f, 0.0028700169641524553f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_4_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(2.3369925022125244f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #8 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_4_weights_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 32,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.002759180963039398f, 0.0025152224116027355f, 0.0026989514008164406f, 0.002645653672516346f, 0.0026487389113754034f, 0.002490818966180086f, 0.0020331910345703363f, 0.0027207043021917343f, 0.002767602913081646f, 0.0024620776530355215f, 0.002991190878674388f, 0.002698577707633376f, 0.0026722217444330454f, 0.002487667603418231f, 0.0028440954629331827f, 0.002689037239179015f, 0.002643930958583951f, 0.0027824281714856625f, 0.0021853430662304163f, 0.0028063664212822914f, 0.0026314686983823776f, 0.002749426756054163f, 0.002580920932814479f, 0.002654171548783779f, 0.002626959001645446f, 0.002463769167661667f, 0.002545410068705678f, 0.0026541706174612045f, 0.0026179661508649588f, 0.002200888702645898f, 0.002756235422566533f, 0.0027092203963547945f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #9 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_5_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(2.618690252304077f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #10 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_5_weights_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 64,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.001883864402770996f, 0.0019385096384212375f, 0.0022430361714214087f, 0.0018852408975362778f, 0.0020153087098151445f, 0.0019554190803319216f, 0.001956445863470435f, 0.0018570390529930592f, 0.0021497635170817375f, 0.001936450251378119f, 0.001962010283023119f, 0.0019154943292960525f, 0.0018937892746180296f, 0.0019612631294876337f, 0.0020981712732464075f, 0.0021851337514817715f, 0.0016336300177499652f, 0.002139420248568058f, 0.0021755637135356665f, 0.0019091321155428886f, 0.0019647039007395506f, 0.002002159832045436f, 0.0018943162867799401f, 0.001960851950570941f, 0.0018777434015646577f, 0.002051900839433074f, 0.0019639988895505667f, 0.0018040105933323503f, 0.0019455894362181425f, 0.0019668941386044025f, 0.0018321590032428503f, 0.00217698747292161f, 0.0019485109951347113f, 0.001991687109693885f, 0.0019406549399718642f, 0.001892442349344492f, 0.002080621663480997f, 0.0021184836514294147f, 0.0018246342660859227f, 0.001893392181955278f, 0.0019246889278292656f, 0.0018410931807011366f, 0.001966522540897131f, 0.001861431053839624f, 0.0019523933297023177f, 0.0020802905783057213f, 0.0020827611442655325f, 0.0018665898824110627f, 0.001804503845050931f, 0.0018711450975388288f, 0.0019538586493581533f, 0.0018882910953834653f, 0.002113842871040106f, 0.002112849149852991f, 0.001904729288071394f, 0.0019670799374580383f, 0.0020172097720205784f, 0.0017877770587801933f, 0.001793202944099903f, 0.0018219446064904332f, 0.0020301630720496178f, 0.0018684357637539506f, 0.0018645637901499867f, 0.00196584383957088f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #11 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_6_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(3.98715877532959f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #12 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(gemm_6_weights_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 3,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.002666484098881483f, 0.002173330867663026f, 0.002290683100000024f),
    AI_PACK_INTQ_ZP(0, 0, 0)))

/**  Tensor declarations section  *********************************************/
/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  conversion_0_output, AI_STATIC,
  0, 0x1,
  AI_SHAPE_INIT(4, 1, 3, 1, 1), AI_STRIDE_INIT(4, 1, 1, 3, 3),
  1, &conversion_0_output_array, &conversion_0_output_array_intq)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  conversion_7_output, AI_STATIC,
  1, 0x0,
  AI_SHAPE_INIT(4, 1, 3, 1, 1), AI_STRIDE_INIT(4, 4, 4, 12, 12),
  1, &conversion_7_output_array, NULL)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  gemm_1_bias, AI_STATIC,
  2, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &gemm_1_bias_array, NULL)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  gemm_1_output, AI_STATIC,
  3, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &gemm_1_output_array, &gemm_1_output_array_intq)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  gemm_1_scratch0, AI_STATIC,
  4, 0x0,
  AI_SHAPE_INIT(4, 1, 323, 1, 1), AI_STRIDE_INIT(4, 2, 2, 646, 646),
  1, &gemm_1_scratch0_array, NULL)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  gemm_1_weights, AI_STATIC,
  5, 0x1,
  AI_SHAPE_INIT(4, 3, 64, 1, 1), AI_STRIDE_INIT(4, 1, 3, 192, 192),
  1, &gemm_1_weights_array, &gemm_1_weights_array_intq)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  gemm_2_bias, AI_STATIC,
  6, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 1, 1), AI_STRIDE_INIT(4, 4, 4, 128, 128),
  1, &gemm_2_bias_array, NULL)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  gemm_2_output, AI_STATIC,
  7, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 1, 1), AI_STRIDE_INIT(4, 1, 1, 32, 32),
  1, &gemm_2_output_array, &gemm_2_output_array_intq)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  gemm_2_scratch0, AI_STATIC,
  8, 0x0,
  AI_SHAPE_INIT(4, 1, 224, 1, 1), AI_STRIDE_INIT(4, 2, 2, 448, 448),
  1, &gemm_2_scratch0_array, NULL)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  gemm_2_weights, AI_STATIC,
  9, 0x1,
  AI_SHAPE_INIT(4, 64, 32, 1, 1), AI_STRIDE_INIT(4, 1, 64, 2048, 2048),
  1, &gemm_2_weights_array, &gemm_2_weights_array_intq)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  gemm_3_bias, AI_STATIC,
  10, 0x0,
  AI_SHAPE_INIT(4, 1, 16, 1, 1), AI_STRIDE_INIT(4, 4, 4, 64, 64),
  1, &gemm_3_bias_array, NULL)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  gemm_3_output, AI_STATIC,
  11, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 1, 1), AI_STRIDE_INIT(4, 1, 1, 16, 16),
  1, &gemm_3_output_array, &gemm_3_output_array_intq)

/* Tensor #12 */
AI_TENSOR_OBJ_DECLARE(
  gemm_3_scratch0, AI_STATIC,
  12, 0x0,
  AI_SHAPE_INIT(4, 1, 112, 1, 1), AI_STRIDE_INIT(4, 2, 2, 224, 224),
  1, &gemm_3_scratch0_array, NULL)

/* Tensor #13 */
AI_TENSOR_OBJ_DECLARE(
  gemm_3_weights, AI_STATIC,
  13, 0x1,
  AI_SHAPE_INIT(4, 32, 16, 1, 1), AI_STRIDE_INIT(4, 1, 32, 512, 512),
  1, &gemm_3_weights_array, &gemm_3_weights_array_intq)

/* Tensor #14 */
AI_TENSOR_OBJ_DECLARE(
  gemm_4_bias, AI_STATIC,
  14, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 1, 1), AI_STRIDE_INIT(4, 4, 4, 128, 128),
  1, &gemm_4_bias_array, NULL)

/* Tensor #15 */
AI_TENSOR_OBJ_DECLARE(
  gemm_4_output, AI_STATIC,
  15, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 1, 1), AI_STRIDE_INIT(4, 1, 1, 32, 32),
  1, &gemm_4_output_array, &gemm_4_output_array_intq)

/* Tensor #16 */
AI_TENSOR_OBJ_DECLARE(
  gemm_4_scratch0, AI_STATIC,
  16, 0x0,
  AI_SHAPE_INIT(4, 1, 176, 1, 1), AI_STRIDE_INIT(4, 2, 2, 352, 352),
  1, &gemm_4_scratch0_array, NULL)

/* Tensor #17 */
AI_TENSOR_OBJ_DECLARE(
  gemm_4_weights, AI_STATIC,
  17, 0x1,
  AI_SHAPE_INIT(4, 16, 32, 1, 1), AI_STRIDE_INIT(4, 1, 16, 512, 512),
  1, &gemm_4_weights_array, &gemm_4_weights_array_intq)

/* Tensor #18 */
AI_TENSOR_OBJ_DECLARE(
  gemm_5_bias, AI_STATIC,
  18, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &gemm_5_bias_array, NULL)

/* Tensor #19 */
AI_TENSOR_OBJ_DECLARE(
  gemm_5_output, AI_STATIC,
  19, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &gemm_5_output_array, &gemm_5_output_array_intq)

/* Tensor #20 */
AI_TENSOR_OBJ_DECLARE(
  gemm_5_scratch0, AI_STATIC,
  20, 0x0,
  AI_SHAPE_INIT(4, 1, 352, 1, 1), AI_STRIDE_INIT(4, 2, 2, 704, 704),
  1, &gemm_5_scratch0_array, NULL)

/* Tensor #21 */
AI_TENSOR_OBJ_DECLARE(
  gemm_5_weights, AI_STATIC,
  21, 0x1,
  AI_SHAPE_INIT(4, 32, 64, 1, 1), AI_STRIDE_INIT(4, 1, 32, 2048, 2048),
  1, &gemm_5_weights_array, &gemm_5_weights_array_intq)

/* Tensor #22 */
AI_TENSOR_OBJ_DECLARE(
  gemm_6_bias, AI_STATIC,
  22, 0x0,
  AI_SHAPE_INIT(4, 1, 3, 1, 1), AI_STRIDE_INIT(4, 4, 4, 12, 12),
  1, &gemm_6_bias_array, NULL)

/* Tensor #23 */
AI_TENSOR_OBJ_DECLARE(
  gemm_6_output, AI_STATIC,
  23, 0x1,
  AI_SHAPE_INIT(4, 1, 3, 1, 1), AI_STRIDE_INIT(4, 1, 1, 3, 3),
  1, &gemm_6_output_array, &gemm_6_output_array_intq)

/* Tensor #24 */
AI_TENSOR_OBJ_DECLARE(
  gemm_6_scratch0, AI_STATIC,
  24, 0x0,
  AI_SHAPE_INIT(4, 1, 79, 1, 1), AI_STRIDE_INIT(4, 2, 2, 158, 158),
  1, &gemm_6_scratch0_array, NULL)

/* Tensor #25 */
AI_TENSOR_OBJ_DECLARE(
  gemm_6_weights, AI_STATIC,
  25, 0x1,
  AI_SHAPE_INIT(4, 64, 3, 1, 1), AI_STRIDE_INIT(4, 1, 64, 192, 192),
  1, &gemm_6_weights_array, &gemm_6_weights_array_intq)

/* Tensor #26 */
AI_TENSOR_OBJ_DECLARE(
  serving_default_keras_tensor0_output, AI_STATIC,
  26, 0x0,
  AI_SHAPE_INIT(4, 1, 3, 1, 1), AI_STRIDE_INIT(4, 4, 4, 12, 12),
  1, &serving_default_keras_tensor0_output_array, NULL)



/**  Layer declarations section  **********************************************/


AI_TENSOR_CHAIN_OBJ_DECLARE(
  conversion_7_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_6_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conversion_7_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  conversion_7_layer, 7,
  NL_TYPE, 0x0, NULL,
  nl, node_convert,
  &conversion_7_chain,
  NULL, &conversion_7_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_6_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_5_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_6_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_6_weights, &gemm_6_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_6_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  gemm_6_layer, 6,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense_integer_SSSA_ch,
  &gemm_6_chain,
  NULL, &conversion_7_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_5_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_4_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_5_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_5_weights, &gemm_5_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_5_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  gemm_5_layer, 5,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense_integer_SSSA_ch,
  &gemm_5_chain,
  NULL, &gemm_6_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_4_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_3_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_4_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_4_weights, &gemm_4_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_4_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  gemm_4_layer, 4,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense_integer_SSSA_ch,
  &gemm_4_chain,
  NULL, &gemm_5_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_3_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_2_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_3_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_3_weights, &gemm_3_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_3_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  gemm_3_layer, 3,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense_integer_SSSA_ch,
  &gemm_3_chain,
  NULL, &gemm_4_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_2_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_1_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_2_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_2_weights, &gemm_2_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_2_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  gemm_2_layer, 2,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense_integer_SSSA_ch,
  &gemm_2_chain,
  NULL, &gemm_3_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  gemm_1_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conversion_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_1_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &gemm_1_weights, &gemm_1_bias),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &gemm_1_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  gemm_1_layer, 1,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense_integer_SSSA_ch,
  &gemm_1_chain,
  NULL, &gemm_2_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  conversion_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &serving_default_keras_tensor0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &conversion_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  conversion_0_layer, 0,
  NL_TYPE, 0x0, NULL,
  nl, node_convert,
  &conversion_0_chain,
  NULL, &gemm_1_layer, AI_STATIC, 
)


#if (AI_TOOLS_API_VERSION < AI_TOOLS_API_VERSION_1_5)

AI_NETWORK_OBJ_DECLARE(
  AI_NET_OBJ_INSTANCE, AI_STATIC,
  AI_BUFFER_INIT(AI_FLAG_NONE,  AI_BUFFER_FORMAT_U8,
    AI_BUFFER_SHAPE_INIT(AI_SHAPE_BCWH, 4, 1, 6348, 1, 1),
    6348, NULL, NULL),
  AI_BUFFER_INIT(AI_FLAG_NONE,  AI_BUFFER_FORMAT_U8,
    AI_BUFFER_SHAPE_INIT(AI_SHAPE_BCWH, 4, 1, 800, 1, 1),
    800, NULL, NULL),
  AI_TENSOR_LIST_IO_OBJ_INIT(AI_FLAG_NONE, AI_NETWORK1_IN_NUM, &serving_default_keras_tensor0_output),
  AI_TENSOR_LIST_IO_OBJ_INIT(AI_FLAG_NONE, AI_NETWORK1_OUT_NUM, &conversion_7_output),
  &conversion_0_layer, 0xcacd12f5, NULL)

#else

AI_NETWORK_OBJ_DECLARE(
  AI_NET_OBJ_INSTANCE, AI_STATIC,
  AI_BUFFER_ARRAY_OBJ_INIT_STATIC(
  	AI_FLAG_NONE, 1,
    AI_BUFFER_INIT(AI_FLAG_NONE,  AI_BUFFER_FORMAT_U8,
      AI_BUFFER_SHAPE_INIT(AI_SHAPE_BCWH, 4, 1, 6348, 1, 1),
      6348, NULL, NULL)
  ),
  AI_BUFFER_ARRAY_OBJ_INIT_STATIC(
  	AI_FLAG_NONE, 1,
    AI_BUFFER_INIT(AI_FLAG_NONE,  AI_BUFFER_FORMAT_U8,
      AI_BUFFER_SHAPE_INIT(AI_SHAPE_BCWH, 4, 1, 800, 1, 1),
      800, NULL, NULL)
  ),
  AI_TENSOR_LIST_IO_OBJ_INIT(AI_FLAG_NONE, AI_NETWORK1_IN_NUM, &serving_default_keras_tensor0_output),
  AI_TENSOR_LIST_IO_OBJ_INIT(AI_FLAG_NONE, AI_NETWORK1_OUT_NUM, &conversion_7_output),
  &conversion_0_layer, 0xcacd12f5, NULL)

#endif	/*(AI_TOOLS_API_VERSION < AI_TOOLS_API_VERSION_1_5)*/



/******************************************************************************/
AI_DECLARE_STATIC
ai_bool network1_configure_activations(
  ai_network* net_ctx, const ai_network_params* params)
{
  AI_ASSERT(net_ctx)

  if (ai_platform_get_activations_map(g_network1_activations_map, 1, params)) {
    /* Updating activations (byte) offsets */
    
    serving_default_keras_tensor0_output_array.data = AI_PTR(g_network1_activations_map[0] + 64);
    serving_default_keras_tensor0_output_array.data_start = AI_PTR(g_network1_activations_map[0] + 64);
    conversion_0_output_array.data = AI_PTR(g_network1_activations_map[0] + 64);
    conversion_0_output_array.data_start = AI_PTR(g_network1_activations_map[0] + 64);
    gemm_1_scratch0_array.data = AI_PTR(g_network1_activations_map[0] + 68);
    gemm_1_scratch0_array.data_start = AI_PTR(g_network1_activations_map[0] + 68);
    gemm_1_output_array.data = AI_PTR(g_network1_activations_map[0] + 0);
    gemm_1_output_array.data_start = AI_PTR(g_network1_activations_map[0] + 0);
    gemm_2_scratch0_array.data = AI_PTR(g_network1_activations_map[0] + 64);
    gemm_2_scratch0_array.data_start = AI_PTR(g_network1_activations_map[0] + 64);
    gemm_2_output_array.data = AI_PTR(g_network1_activations_map[0] + 512);
    gemm_2_output_array.data_start = AI_PTR(g_network1_activations_map[0] + 512);
    gemm_3_scratch0_array.data = AI_PTR(g_network1_activations_map[0] + 0);
    gemm_3_scratch0_array.data_start = AI_PTR(g_network1_activations_map[0] + 0);
    gemm_3_output_array.data = AI_PTR(g_network1_activations_map[0] + 224);
    gemm_3_output_array.data_start = AI_PTR(g_network1_activations_map[0] + 224);
    gemm_4_scratch0_array.data = AI_PTR(g_network1_activations_map[0] + 240);
    gemm_4_scratch0_array.data_start = AI_PTR(g_network1_activations_map[0] + 240);
    gemm_4_output_array.data = AI_PTR(g_network1_activations_map[0] + 0);
    gemm_4_output_array.data_start = AI_PTR(g_network1_activations_map[0] + 0);
    gemm_5_scratch0_array.data = AI_PTR(g_network1_activations_map[0] + 32);
    gemm_5_scratch0_array.data_start = AI_PTR(g_network1_activations_map[0] + 32);
    gemm_5_output_array.data = AI_PTR(g_network1_activations_map[0] + 736);
    gemm_5_output_array.data_start = AI_PTR(g_network1_activations_map[0] + 736);
    gemm_6_scratch0_array.data = AI_PTR(g_network1_activations_map[0] + 0);
    gemm_6_scratch0_array.data_start = AI_PTR(g_network1_activations_map[0] + 0);
    gemm_6_output_array.data = AI_PTR(g_network1_activations_map[0] + 160);
    gemm_6_output_array.data_start = AI_PTR(g_network1_activations_map[0] + 160);
    conversion_7_output_array.data = AI_PTR(g_network1_activations_map[0] + 0);
    conversion_7_output_array.data_start = AI_PTR(g_network1_activations_map[0] + 0);
    return true;
  }
  AI_ERROR_TRAP(net_ctx, INIT_FAILED, NETWORK_ACTIVATIONS);
  return false;
}




/******************************************************************************/
AI_DECLARE_STATIC
ai_bool network1_configure_weights(
  ai_network* net_ctx, const ai_network_params* params)
{
  AI_ASSERT(net_ctx)

  if (ai_platform_get_weights_map(g_network1_weights_map, 1, params)) {
    /* Updating weights (byte) offsets */
    
    gemm_1_weights_array.format |= AI_FMT_FLAG_CONST;
    gemm_1_weights_array.data = AI_PTR(g_network1_weights_map[0] + 0);
    gemm_1_weights_array.data_start = AI_PTR(g_network1_weights_map[0] + 0);
    gemm_1_bias_array.format |= AI_FMT_FLAG_CONST;
    gemm_1_bias_array.data = AI_PTR(g_network1_weights_map[0] + 192);
    gemm_1_bias_array.data_start = AI_PTR(g_network1_weights_map[0] + 192);
    gemm_2_weights_array.format |= AI_FMT_FLAG_CONST;
    gemm_2_weights_array.data = AI_PTR(g_network1_weights_map[0] + 448);
    gemm_2_weights_array.data_start = AI_PTR(g_network1_weights_map[0] + 448);
    gemm_2_bias_array.format |= AI_FMT_FLAG_CONST;
    gemm_2_bias_array.data = AI_PTR(g_network1_weights_map[0] + 2496);
    gemm_2_bias_array.data_start = AI_PTR(g_network1_weights_map[0] + 2496);
    gemm_3_weights_array.format |= AI_FMT_FLAG_CONST;
    gemm_3_weights_array.data = AI_PTR(g_network1_weights_map[0] + 2624);
    gemm_3_weights_array.data_start = AI_PTR(g_network1_weights_map[0] + 2624);
    gemm_3_bias_array.format |= AI_FMT_FLAG_CONST;
    gemm_3_bias_array.data = AI_PTR(g_network1_weights_map[0] + 3136);
    gemm_3_bias_array.data_start = AI_PTR(g_network1_weights_map[0] + 3136);
    gemm_4_weights_array.format |= AI_FMT_FLAG_CONST;
    gemm_4_weights_array.data = AI_PTR(g_network1_weights_map[0] + 3200);
    gemm_4_weights_array.data_start = AI_PTR(g_network1_weights_map[0] + 3200);
    gemm_4_bias_array.format |= AI_FMT_FLAG_CONST;
    gemm_4_bias_array.data = AI_PTR(g_network1_weights_map[0] + 3712);
    gemm_4_bias_array.data_start = AI_PTR(g_network1_weights_map[0] + 3712);
    gemm_5_weights_array.format |= AI_FMT_FLAG_CONST;
    gemm_5_weights_array.data = AI_PTR(g_network1_weights_map[0] + 3840);
    gemm_5_weights_array.data_start = AI_PTR(g_network1_weights_map[0] + 3840);
    gemm_5_bias_array.format |= AI_FMT_FLAG_CONST;
    gemm_5_bias_array.data = AI_PTR(g_network1_weights_map[0] + 5888);
    gemm_5_bias_array.data_start = AI_PTR(g_network1_weights_map[0] + 5888);
    gemm_6_weights_array.format |= AI_FMT_FLAG_CONST;
    gemm_6_weights_array.data = AI_PTR(g_network1_weights_map[0] + 6144);
    gemm_6_weights_array.data_start = AI_PTR(g_network1_weights_map[0] + 6144);
    gemm_6_bias_array.format |= AI_FMT_FLAG_CONST;
    gemm_6_bias_array.data = AI_PTR(g_network1_weights_map[0] + 6336);
    gemm_6_bias_array.data_start = AI_PTR(g_network1_weights_map[0] + 6336);
    return true;
  }
  AI_ERROR_TRAP(net_ctx, INIT_FAILED, NETWORK_WEIGHTS);
  return false;
}


/**  PUBLIC APIs SECTION  *****************************************************/



AI_DEPRECATED
AI_API_ENTRY
ai_bool ai_network1_get_info(
  ai_handle network, ai_network_report* report)
{
  ai_network* net_ctx = AI_NETWORK_ACQUIRE_CTX(network);

  if (report && net_ctx)
  {
    ai_network_report r = {
      .model_name        = AI_NETWORK1_MODEL_NAME,
      .model_signature   = AI_NETWORK1_MODEL_SIGNATURE,
      .model_datetime    = AI_TOOLS_DATE_TIME,
      
      .compile_datetime  = AI_TOOLS_COMPILE_TIME,
      
      .runtime_revision  = ai_platform_runtime_get_revision(),
      .runtime_version   = ai_platform_runtime_get_version(),

      .tool_revision     = AI_TOOLS_REVISION_ID,
      .tool_version      = {AI_TOOLS_VERSION_MAJOR, AI_TOOLS_VERSION_MINOR,
                            AI_TOOLS_VERSION_MICRO, 0x0},
      .tool_api_version  = AI_STRUCT_INIT,

      .api_version            = ai_platform_api_get_version(),
      .interface_api_version  = ai_platform_interface_api_get_version(),
      
      .n_macc            = 5727,
      .n_inputs          = 0,
      .inputs            = NULL,
      .n_outputs         = 0,
      .outputs           = NULL,
      .params            = AI_STRUCT_INIT,
      .activations       = AI_STRUCT_INIT,
      .n_nodes           = 0,
      .signature         = 0xcacd12f5,
    };

    if (!ai_platform_api_get_network_report(network, &r)) return false;

    *report = r;
    return true;
  }
  return false;
}



AI_API_ENTRY
ai_bool ai_network1_get_report(
  ai_handle network, ai_network_report* report)
{
  ai_network* net_ctx = AI_NETWORK_ACQUIRE_CTX(network);

  if (report && net_ctx)
  {
    ai_network_report r = {
      .model_name        = AI_NETWORK1_MODEL_NAME,
      .model_signature   = AI_NETWORK1_MODEL_SIGNATURE,
      .model_datetime    = AI_TOOLS_DATE_TIME,
      
      .compile_datetime  = AI_TOOLS_COMPILE_TIME,
      
      .runtime_revision  = ai_platform_runtime_get_revision(),
      .runtime_version   = ai_platform_runtime_get_version(),

      .tool_revision     = AI_TOOLS_REVISION_ID,
      .tool_version      = {AI_TOOLS_VERSION_MAJOR, AI_TOOLS_VERSION_MINOR,
                            AI_TOOLS_VERSION_MICRO, 0x0},
      .tool_api_version  = AI_STRUCT_INIT,

      .api_version            = ai_platform_api_get_version(),
      .interface_api_version  = ai_platform_interface_api_get_version(),
      
      .n_macc            = 5727,
      .n_inputs          = 0,
      .inputs            = NULL,
      .n_outputs         = 0,
      .outputs           = NULL,
      .map_signature     = AI_MAGIC_SIGNATURE,
      .map_weights       = AI_STRUCT_INIT,
      .map_activations   = AI_STRUCT_INIT,
      .n_nodes           = 0,
      .signature         = 0xcacd12f5,
    };

    if (!ai_platform_api_get_network_report(network, &r)) return false;

    *report = r;
    return true;
  }
  return false;
}


AI_API_ENTRY
ai_error ai_network1_get_error(ai_handle network)
{
  return ai_platform_network_get_error(network);
}


AI_API_ENTRY
ai_error ai_network1_create(
  ai_handle* network, const ai_buffer* network_config)
{
  return ai_platform_network_create(
    network, network_config, 
    AI_CONTEXT_OBJ(&AI_NET_OBJ_INSTANCE),
    AI_TOOLS_API_VERSION_MAJOR, AI_TOOLS_API_VERSION_MINOR, AI_TOOLS_API_VERSION_MICRO);
}


AI_API_ENTRY
ai_error ai_network1_create_and_init(
  ai_handle* network, const ai_handle activations[], const ai_handle weights[])
{
  ai_error err;
  ai_network_params params;

  err = ai_network1_create(network, AI_NETWORK1_DATA_CONFIG);
  if (err.type != AI_ERROR_NONE) {
    return err;
  }
  
  if (ai_network1_data_params_get(&params) != true) {
    err = ai_network1_get_error(*network);
    return err;
  }
#if defined(AI_NETWORK1_DATA_ACTIVATIONS_COUNT)
  /* set the addresses of the activations buffers */
  for (ai_u16 idx=0; activations && idx<params.map_activations.size; idx++) {
    AI_BUFFER_ARRAY_ITEM_SET_ADDRESS(&params.map_activations, idx, activations[idx]);
  }
#endif
#if defined(AI_NETWORK1_DATA_WEIGHTS_COUNT)
  /* set the addresses of the weight buffers */
  for (ai_u16 idx=0; weights && idx<params.map_weights.size; idx++) {
    AI_BUFFER_ARRAY_ITEM_SET_ADDRESS(&params.map_weights, idx, weights[idx]);
  }
#endif
  if (ai_network1_init(*network, &params) != true) {
    err = ai_network1_get_error(*network);
  }
  return err;
}


AI_API_ENTRY
ai_buffer* ai_network1_inputs_get(ai_handle network, ai_u16 *n_buffer)
{
  if (network == AI_HANDLE_NULL) {
    network = (ai_handle)&AI_NET_OBJ_INSTANCE;
    AI_NETWORK_OBJ(network)->magic = AI_MAGIC_CONTEXT_TOKEN;
  }
  return ai_platform_inputs_get(network, n_buffer);
}


AI_API_ENTRY
ai_buffer* ai_network1_outputs_get(ai_handle network, ai_u16 *n_buffer)
{
  if (network == AI_HANDLE_NULL) {
    network = (ai_handle)&AI_NET_OBJ_INSTANCE;
    AI_NETWORK_OBJ(network)->magic = AI_MAGIC_CONTEXT_TOKEN;
  }
  return ai_platform_outputs_get(network, n_buffer);
}


AI_API_ENTRY
ai_handle ai_network1_destroy(ai_handle network)
{
  return ai_platform_network_destroy(network);
}


AI_API_ENTRY
ai_bool ai_network1_init(
  ai_handle network, const ai_network_params* params)
{
  ai_network* net_ctx = AI_NETWORK_OBJ(ai_platform_network_init(network, params));
  ai_bool ok = true;

  if (!net_ctx) return false;
  ok &= network1_configure_weights(net_ctx, params);
  ok &= network1_configure_activations(net_ctx, params);

  ok &= ai_platform_network_post_init(network);

  return ok;
}


AI_API_ENTRY
ai_i32 ai_network1_run(
  ai_handle network, const ai_buffer* input, ai_buffer* output)
{
  return ai_platform_network_process(network, input, output);
}


AI_API_ENTRY
ai_i32 ai_network1_forward(ai_handle network, const ai_buffer* input)
{
  return ai_platform_network_process(network, input, NULL);
}



#undef AI_NETWORK1_MODEL_SIGNATURE
#undef AI_NET_OBJ_INSTANCE
#undef AI_TOOLS_DATE_TIME
#undef AI_TOOLS_COMPILE_TIME

