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
 * hcom4db_utils.h
 *
 *
 * IDENTIFICATION
 *    src/hcom4db_utils.h
 *
 * -------------------------------------------------------------------------
 */

#ifndef HCOM4DB_UTILS_H
#define HCOM4DB_UTILS_H

#include <cstdint>
#include <string>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

uint32_t Calculate2PowerTimes(uint32_t powerNum);

int Hcom4dbLoadSymbol(void *libHandle, std::string symbol, void **symLibHandle);

int Hcom4dbOpenDl(void **libHandle, std::string path);

void Hcom4dbCloseDl(void *libHandle);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif