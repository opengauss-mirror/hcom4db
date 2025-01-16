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
 * hcom4db_define.h
 *
 *
 * IDENTIFICATION
 *    src/hcom4db_define.h
 *
 * -------------------------------------------------------------------------
 */

#ifndef HCOM4DB_DEFINE_H
#define HCOM4DB_DEFINE_H
#include "hcom4db_hcom.h"
#include "hcom4db.h"
#ifdef __cplusplus
#if __cplusplus
#endif
extern "C" {
#endif
#endif

namespace hcom4dbDefine{
using TlsFuncs = struct TlsFunc {
    OckRpcTlsGetCAAndVerify getCaAndVerify; /* get the CA path and verify callback */
    OckRpcTlsGetCert getCert;               /* get the certifycate file of public key */
    OckRpcTlsGetPrivateKey getPriKey;       /* get the private key and key pass */
    OckRpcTlsKeypassErase eraseKeyPass;
};

using AsyncCbInfo = struct {
    OckRpcDoneCallback cb;
    void *arg;
    OckRpcClient client;
    OckRpcMessage *rsp;
    uintptr_t *reqAddr;     // Set to uintptr for sgl only, otherwise, set to nullptr
    uint32_t reqCnt;
};

using RawMessageHeader = struct {
    uint16_t msgId;
};

using OckRpcMrInfo = struct OckRpcMrInfo {
    Service_MemoryRegion mr;
    Service_MemoryRegionInfo mrInfo;
    OckRpcServer server;
};
}   // namespace hcom4dbDefine

#ifdef __cplusplus
#if __cplusplus
}
#endif

#endif