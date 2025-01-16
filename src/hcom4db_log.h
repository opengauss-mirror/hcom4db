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
 * hcom4db_log.h
 *
 *
 * IDENTIFICATION
 *    src/hcom4db_log.h
 *
 * -------------------------------------------------------------------------
 */

#ifndef HCOM4DB_LOG_H
#define HCOM4DB_LOG_H

#include <iostream>
#include <sstream>
#include <mutex>

#ifdef __cplusplus
#if __cplusplus
#endif
extern "C" {
#endif

#ifndef OCK_RPC_LIKELY
#define OCK_RPC_LIKELY(x) (__builtin_expect(!!(x), 1) != 0)
#endif

#ifndef OCK_RPC_UNLIKELY
#define OCK_RPC_UNLIKELY(x) (__builtin_expect(!!(x), 0) != 0)
#endif

typedef void (*ExternalLog)(int level, const char *msg);

class Hcom4dbLog {
public:
    static Hcom4dbLog *Instance();
    static void SetLogLevel();
    static void SetLogLevel(int level);
    static bool SetStrStol(const std::string &str, long &value);

    inline int GetLogLevel() const
    {
        return logLevel;
    }

    inline void SetExternalLogFunction(ExternalLog func)
    {
        mLogFunc = func;
    }

    void Log(int level, const std::ostringstream &oss) const;

    Hcom4dbLog(const Hcom4dbLog &) = delete;
    Hcom4dbLog &operator = (const Hcom4dbLog &) = delete;
    Hcom4dbLog(Hcom4dbLog &&) = delete;
    Hcom4dbLog &operator = (Hcom4dbLog &&) = delete;

    ~Hcom4dbLog()
    {
        mLogFunc = nullptr;
    }

private:
    Hcom4dbLog() = default;

private:
    static Hcom4dbLog *gLogger;
    static std::mutex gMutex;
    static int logLevel;

    ExternalLog mLogFunc = nullptr;
};

// macro for log
#ifndef OCK_RPC_FILENAME
#define OCK_RPC_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif
#define OCK_RPC_LOG(level, args)                                                                \
    do {                                                                                        \
        if ((level) >= (Hcom4dbLog::Instance()->GetLogLevel())) {                               \
            std::ostringstream oss;                                                             \
            if ((level) == 0) {                                                                 \
                oss << "[DEBUG]";                                                               \
            } else if ((level) == 1) {                                                          \
                oss << "[INFO]";                                                                \
            } else if ((level) == 2) {                                                          \
                oss << "[WARN]";                                                                \
            } else if ((level) == 3) {                                                          \
                oss << "[ERROR]";                                                               \
            }                                                                                   \
            oss << "[HCOM4DB " << OCK_RPC_FILENAME << ":" << __LINE__ << "] (" << args << ")";  \
            Hcom4dbLog::Instance()->Log(level, oss);                                            \
        }                                                                                       \
    } while (0)                                                                                 \

#define OCK_RPC_HCOM_LOG(level, args)                                                           \
    do {                                                                                        \
        if ((level) >= (Hcom4dbLog::Instance()->GetLogLevel())) {                               \
            std::ostringstream oss;                                                             \
            if ((level) == 0) {                                                                 \
                oss << "[DEBUG]";                                                               \
            } else if ((level) == 1) {                                                          \
                oss << "[INFO]";                                                                \
            } else if ((level) == 2) {                                                          \
                oss << "[WARN]";                                                                \
            } else if ((level) == 3) {                                                          \
                oss << "[ERROR]";                                                               \
            }                                                                                   \
            oss << (args);                                                                      \
            Hcom4dbLog::Instance()->Log(level, oss);                                            \
        }                                                                                       \
    } while (0)                                                                                 \

#define OCK_RPC_LOG_DEBUG(args) OCK_RPC_LOG(0, args)
#define OCK_RPC_LOG_INFO(args) OCK_RPC_LOG(1, args)
#define OCK_RPC_LOG_WARN(args) OCK_RPC_LOG(2, args)
#define OCK_RPC_LOG_ERROR(args) OCK_RPC_LOG(3, args)

#define OCK_RPC_ASSERT_LOG_RETURN(args, RET)    \
    if (OCK_RPC_UNLIKELY(!(args))) {            \
        OCK_RPC_LOG_ERROR("Assert " << #args);  \
        return RET;                             \
    }                                           \

#define OCK_RPC_ASSERT_LOG_RETURN_VOID(args)    \
    if (OCK_RPC_UNLIKELY(!(args))) {            \
        OCK_RPC_LOG_ERROR("Assert " << #args);  \
        return;                                 \
    }                                           \

#ifdef OCK_RPC_TRACE_INFO_ENABLED
#define OCK_RPC_TRACE_INFO(args) OCK_RPC_LOG_INFO(args)
#else
#define OCK_RPC_TRACE_INFO(x)
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif