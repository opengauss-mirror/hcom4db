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
 * hcom4db_log.cpp
 *
 *
 * IDENTIFICATION
 *    src/hcom4db_log.cpp
 *
 * -------------------------------------------------------------------------
 */

#include <ctime>
#include <sys/time.h>
#include <cstring>
#include "hcom4db_log.h"

namespace {
    constexpr std::uint16_t LOG_LEVEL_DEBUG = 0;
    constexpr std::uint16_t LOG_LEVEL_INFO = 1;
    constexpr std::uint16_t LOG_LEVEL_WARN = 2;
    constexpr std::uint16_t LOG_LEVEL_ERROR = 3;
} /* namespace */

Hcom4dbLog *Hcom4dbLog::gLogger = nullptr;
std::mutex Hcom4dbLog::gMutex;
int Hcom4dbLog::logLevel = LOG_LEVEL_INFO;

Hcom4dbLog *Hcom4dbLog::Instance()
{
    if (OCK_RPC_UNLIKELY(gLogger == nullptr)) {
        std::lock_guard<std::mutex> lock(gMutex);
        if (gLogger == nullptr) {
            gLogger = new (std::nothrow) Hcom4dbLog();
            if (gLogger == nullptr) {
                printf("Failed to new Hcom4dbLog, probably out of memory");
            }
            SetLogLevel();
        }
    }

    return gLogger;
}

void Hcom4dbLog::Log(int level, const std::ostringstream &oss) const
{
    if (OCK_RPC_LIKELY(mLogFunc != nullptr)) {
        mLogFunc(level, oss.str().c_str());
    } else {
        struct timeval tv {};
        char strTime[24];

        gettimeofday(&tv, nullptr);
        time_t timeStamp = tv.tv_sec;
        struct tm localTime {};
        if (strftime(strTime, sizeof strTime, "%Y-%m-%d %H:%M:%S.", localtime_r(&timeStamp, &localTime)) != 0) {
            std::cout << strTime << tv.tv_usec << " " << level << " " << oss.str() << std::endl;
        } else {
            std::cout << "Invalid time trace" << tv.tv_usec << " " << level << " " << oss.str() << std::endl;
        }
    }
}

void Hcom4dbLog::SetLogLevel()
{
    /* set one of 0,1,2,3 */
    char *envSize = ::getenv("HCOM_SET_LOG_LEVEL");
    if (envSize != nullptr) {
        long value = 0;
        if (!SetStrStol(envSize, value)) {
            std::cout << "Invalid setting 'HCOM_SET_LOG_LEVEL', should set one of 0,1,2,3 "  << std::endl;
            return;
        }
        std::cout << "Setting 'HCOM_SET_LOG_LEVEL' success, level:"  << std::endl;
        logLevel = value;
    }
}

void Hcom4dbLog::SetLogLevel(int level)
{
    auto value = static_cast<uint32_t>(level);
    if (value <= LOG_LEVEL_ERROR) {
        logLevel = level;
    }
}

bool Hcom4dbLog::SetStrStol(const std::string &str, long &value)
{
    char *remain = nullptr;
    errno = 0;
    value = std::strtol(str.c_str(), &remain, 10); // 10 is decimal digits
    if (remain == nullptr || strlen(remain) > 0 || value < LOG_LEVEL_DEBUG || value > LOG_LEVEL_ERROR ||
        errno == ERANGE) {
        return false;
    } else if (value == 0 && str != "0") {
        return false;
    }

    return true;
}