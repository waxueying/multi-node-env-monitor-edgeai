/**
  ******************************************************************************
  * @file    network1_data_params.h
  * @author  AST Embedded Analytics Research Platform
  * @date    2025-12-02T14:52:04+0800
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#ifndef NETWORK1_DATA_PARAMS_H
#define NETWORK1_DATA_PARAMS_H

#include "ai_platform.h"

/*
#define AI_NETWORK1_DATA_WEIGHTS_PARAMS \
  (AI_HANDLE_PTR(&ai_network1_data_weights_params[1]))
*/

#define AI_NETWORK1_DATA_CONFIG               (NULL)


#define AI_NETWORK1_DATA_ACTIVATIONS_SIZES \
  { 800, }
#define AI_NETWORK1_DATA_ACTIVATIONS_SIZE     (800)
#define AI_NETWORK1_DATA_ACTIVATIONS_COUNT    (1)
#define AI_NETWORK1_DATA_ACTIVATION_1_SIZE    (800)



#define AI_NETWORK1_DATA_WEIGHTS_SIZES \
  { 6348, }
#define AI_NETWORK1_DATA_WEIGHTS_SIZE         (6348)
#define AI_NETWORK1_DATA_WEIGHTS_COUNT        (1)
#define AI_NETWORK1_DATA_WEIGHT_1_SIZE        (6348)



#define AI_NETWORK1_DATA_ACTIVATIONS_TABLE_GET() \
  (&g_network1_activations_table[1])

extern ai_handle g_network1_activations_table[1 + 2];



#define AI_NETWORK1_DATA_WEIGHTS_TABLE_GET() \
  (&g_network1_weights_table[1])

extern ai_handle g_network1_weights_table[1 + 2];


#endif    /* NETWORK1_DATA_PARAMS_H */
