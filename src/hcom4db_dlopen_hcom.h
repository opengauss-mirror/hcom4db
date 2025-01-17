/*
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * CBB is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 *
 * hcom4db_dlopen_hcom.h
 *
 *
 * IDENTIFICATION
 *    src/hcom4db_dlopen_hcom.h
 *
 * -------------------------------------------------------------------------
 */

#ifndef HCOM4DB_DLOPEN_H
#define HCOM4DB_DLOPEN_H

#include <cstdio>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Dlopen libhcom.so, regitser function of hcom4db_hcom.h
 *
 * @param path      [in] the path of libhcom.so
 * @param pathLen   [in] the path len
 *
 * @return 0 for success and -1 for failed
 */
int InitOckRpcDl(char *path, unsigned int pathLen);

/**
 * @brief Close dlopen for libhcom.so
 */
void FinishOckRpcDl(void);

#ifdef __cplusplus
}
#endif

#endif