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
 * hcom4db_utils.cpp
 *
 *
 * IDENTIFICATION
 *    src/hcom4db_utils.cpp
 *
 * -------------------------------------------------------------------------
 */

#include <dlfcn.h>
#include <iostream>
#include "hcom4db_utils.h"

uint32_t Calculate2PowerTimes(uint32_t powerNum)
{
    uint32_t count = 0;
    while (powerNum != 0) {
        powerNum = powerNum >> 1;
        count++;
    }

    return count;
}

int Hcom4dbLoadSymbol(void *libHandle, std::string symbol, void **symLibHandle)
{
    const char *dlsymErr = nullptr;

    *symLibHandle = dlsym(libHandle, const_cast<char *>(symbol.c_str()));
    dlsymErr = dlerror();
    if (dlsymErr != nullptr) {
        std::cerr << "Load symbol error: " << dlsymErr << std::endl;
        return -1;
    }

    return 0;
}

int Hcom4dbOpenDl(void **libHandle, std::string path)
{
    *libHandle = dlopen(const_cast<char *>(path.c_str()), RTLD_LAZY);
    if (*libHandle == nullptr) {
        std::cerr << "Open path error: " << errno << ", error message: " << dlerror() << std::endl;
        return -1;
    }

    return 0;
}

void Hcom4dbCloseDl(void *libHandle)
{
    (void)dlclose(libHandle);
}