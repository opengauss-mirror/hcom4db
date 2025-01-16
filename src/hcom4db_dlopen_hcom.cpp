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
 * hcom4db_dlopen_hcom.cpp
 *
 *
 * IDENTIFICATION
 *    src/hcom4db_dlopen_hcom.cpp
 *
 * -------------------------------------------------------------------------
 */

#include "securec.h"
#include "hcom4db_utils.h"
#include "hcom4db_log.h"
#include "hcom4db.h"
#include "hcom4db_hcom.h"
#include "hcom4db_dlopen_hcom.h"

using GetChannel = int (*)(Service_Context context, Net_Channel *channel);
using GetContextType = int (*)(Service_Context context, Service_ContextType *type);
using GetResult = int (*)(Service_Context context, int *result);
using GetRspCtx = int (*)(Service_Context context, Service_RspCtx *rsp);
using GetOpInfo = int (*)(Service_Context context, Service_OpInfo *info);
using GetMessageData = void *(*)(Service_Context context);
using GetMessageDataLen = uint32_t (*)(Service_Context context);
using GetRndvChannel = int (*)(Service_RndvContext context, Net_Channel *channel);
using GetRndvCtxType = int (*)(Service_RndvContext context, Service_RndvType *type);
using GetRndvOpInfo = int (*)(Service_RndvContext context, Service_OpInfo *info);
using GetRndvMessage = int (*)(Service_RndvContext context, Service_RndvMessage *message);
using RndvFreeContext = int (*)(Service_RndvContext context);
using RndvReply = int (*)(Service_RndvContext context, Service_OpInfo *opInfo, Service_Message *message,
                          Channel_Callback *cb);
using SetUpCtx = int (*)(Net_Channel channel, uint64_t ctx);
using GetUpCtx = int (*)(Net_Channel channel, uint64_t *ctx);
using PostSend = int (*)(Net_Channel channel, Service_OpInfo *opInfo, Service_Message *message, Channel_Callback *cb);
using PostResponse = int (*)(Net_Channel channel, Service_RspCtx rspCtx, Service_OpInfo *opInfo, Service_Message *message,
                             Channel_Callback *cb);
using PostSendRaw = int (*)(Net_Channel channel, Service_Message *message, Channel_Callback *cb);
using PostResponseRaw = int (*)(Net_Channel channel, Service_RspCtx rspCtx, Service_Message *message, Channel_Callback *cb);
using PostSendRawSgl = int (*)(Net_Channel channel, Service_SglRequest *message, Channel_Callback *cb);
using PostResponseRawSgl = int (*)(Net_Channel channel, Service_RspCtx rspCtx, Service_SglRequest *message,
                                   Channel_Callback *cb);
using SyncCall = int (*)(Net_Channel channel, Service_OpInfo *reqInfo, Service_Message *req, Service_OpInfo *rspInfo,
                         Service_Message *rsp);
using AsyncCall = int (*)(Net_Channel channel, Service_OpInfo *opInfo, Service_Message *req, Channel_Callback *cb);
using SyncCallRaw = int (*)(Net_Channel channel, Service_Message *req, Service_Message *rsp);
using AsyncCallRaw = int (*)(Net_Channel channel, Service_Message *req, Channel_Callback *cb);
using SyncCallRawSgl = int (*)(Net_Channel channel, Service_SglRequest *req, Service_Message *rsp);
using AsyncCallRawSgl = int (*)(Net_Channel channel, Service_SglRequest *req, Channel_Callback *cb);
using SyncRndvCall = int (*)(Net_Channel channel, Service_OpInfo *reqInfo, Service_Request *req, Service_OpInfo *rspInfo,
                             Service_Message *rsp);
using AsyncRndvCall = int (*)(Net_Channel channel, Service_OpInfo *reqInfo, Service_Request *req, Channel_Callback *cb);
using SyncRndvSglCall = int (*)(Net_Channel channel, Service_OpInfo *reqInfo, Service_SglRequest *req,
                                Service_OpInfo *rspInfo, Service_Message *rsp);
using AsyncRndvSglCall = int (*)(Net_Channel channel, Service_OpInfo *reqInfo, Service_SglRequest *req,
                                 Channel_Callback *cb);
using Read = int (*)(Net_Channel channel, Service_Request *req, Channel_Callback *cb);
using ReadSgl = int (*)(Net_Channel channel, Service_SglRequest *req, Channel_Callback *cb);
using Write = int (*)(Net_Channel channel, Service_Request *req, Channel_Callback *cb);
using WriteSgl = int (*)(Net_Channel channel, Service_SglRequest *req, Channel_Callback *cb);
using ChannelClose = void (*)(Net_Channel channel);
using SetOneSideTimeout = void (*)(Net_Channel channel, int32_t timeout);
using SetTwoSideTimeout = void (*)(Net_Channel channel, int32_t timeout);
using SetExternalLogger = void (*)(Service_LogHandler h);
using ServiceCreate = int (*)(Service_Type t, const char *name, uint8_t startOobSvr, Net_Service *service);
using SetOobIpAndPort = void (*)(Net_Service service, const char *ip, uint16_t port);
using GetOobIpAndPort = bool (*)(Net_Service service, char ***ipArray, uint16_t **portArray, int *length);
using ServiceStart = int (*)(Net_Service service, Service_Options options);
using ServiceStop = void (*)(Net_Service service);
using ServiceDestroy = int (*)(Net_Service service);
using ChannelConnect = int (*)(Net_Service service, const char *oobIpOrName, uint16_t oobPort, const char *payload,
                               Net_Channel *channel, Service_ConnectOpt *options);
using ChannelDeRefer = void (*)(Net_Channel channel);
using ChannelDestroy = void (*)(Net_Channel channel);
using RegisterMemoryRegion = int (*)(Net_Service service, uint64_t size, Service_MemoryRegion *mr);
using RegisterAssignMemoryRegion = int (*)(Net_Service service, uintptr_t address, uint64_t size, Service_MemoryRegion *mr);
using GetMemoryRegionInfo = int (*)(Service_MemoryRegion mr, Service_MemoryRegionInfo *info);
using DestroyMemoryRegion = int (*)(Net_Service service, Service_MemoryRegion mr);
using RegisterChannelHandler = uintptr_t (*)(Net_Service service, Service_HandlerType t, Service_ChannelHandler h,
                                             Service_ChannelPolicy policy, uint64_t usrCtx);
using RegisterOpHandler = uintptr_t (*)(Net_Service service, uint16_t i, Service_OpHandlerType t, Service_RequestHandler h,
                                        uint64_t usrCtx);
using RegisterIdleHandler = uintptr_t (*)(Net_Service service, Service_IdleHandler h, uint64_t usrCtx);
using RegisterAllocateHandler = uintptr_t (*)(Net_Service service, Service_RndvMemAllocate h);
using RegisterFreeHandler = uintptr_t (*)(Net_Service service, Service_RndvMemFree h);
using RegisterRndvOpHandler = uintptr_t (*)(Net_Service service, Service_RndvHandler h);
using RegisterTLSCb = uintptr_t (*)(Net_Service service, Net_TlsGetCertCb certCb, Net_TlsGetPrivateKeyCb priKeyCb,
                                    Net_TlsGetCACb caCb);
using MemoryAllocatorCreate = int (*)(
    Net_MemoryAllocatorType t, Net_MemoryAllocatorOptions *options, Net_MemoryAllocator *allocator);
using MemoryAllocatorDestroy = int (*)(Net_MemoryAllocator allocator);
using MemoryAllocatorSetMrKey = int (*)(Net_MemoryAllocator allocator, uint32_t mrKey);
using MemoryAllocatorMemOffset = int (*)(Net_MemoryAllocator allocator, uintptr_t address, uintptr_t *offset);
using MemoryAllocatorFreeSize = int (*)(Net_MemoryAllocator allocator, uintptr_t *size);
using MemoryAllocatorAllocate = int (*)(Net_MemoryAllocator allocator, uint64_t size, uintptr_t *address, uint32_t *key);
using MemoryAllocatorFree = int (*)(Net_MemoryAllocator allocator, uintptr_t address);
using SetExternalLogger = void (*)(Net_LogHandler h);

using HcomFunc = struct {
    GetChannel getChannel;
    GetContextType getContextType;
    GetResult getResult;
    GetRspCtx getRspCtx;
    GetOpInfo getOpInfo;
    GetMessageData getMessageData;
    GetMessageDataLen getMessageDataLen;
    GetRndvChannel getRndvChannel;
    GetRndvCtxType getRndvCtxType;
    GetRndvOpInfo getRndvOpInfo;
    GetRndvMessage getRndvMessage;
    RndvFreeContext rndvFreeContext;
    RndvReply rndvReply;
    SetUpCtx setUpCtx;
    GetUpCtx getUpCtx;
    PostSend postSend;
    PostResponse postResponse;
    PostSendRaw postSendRaw;
    PostResponseRaw postResponseRaw;
    PostSendRawSgl postSendRawSgl;
    PostResponseRawSgl postResponseRawSgl;
    SyncCall syncCall;
    AsyncCall asyncCall;
    SyncCallRaw syncCallRaw;
    AsyncCallRaw asyncCallRaw;
    SyncCallRawSgl syncCallRawSgl;
    AsyncCallRawSgl asyncCallRawSgl;
    SyncRndvCall syncRndvCall;
    AsyncRndvCall asyncRndvCall;
    SyncRndvSglCall syncRndvSglCall;
    AsyncRndvSglCall asyncRndvSglCall;
    Read read;
    ReadSgl readSgl;
    Write write;
    WriteSgl writeSgl;
    ChannelClose channelClose;
    SetOneSideTimeout setOneSideTimeout;
    SetTwoSideTimeout setTwoSideTimeout;
    SetExternalLogger setExternalLogger;
    ServiceCreate serviceCreate;
    SetOobIpAndPort setOobIpAndPort;
    GetOobIpAndPort getOobIpAndPort;
    ServiceStart serviceStart;
    ServiceStop serviceStop;
    ServiceDestroy serviceDestroy;
    ChannelConnect channelConnect;
    ChannelDeRefer channelDeRefer;
    ChannelDestroy channelDestroy;
    RegisterMemoryRegion registerMemoryRegion;
    RegisterAssignMemoryRegion registerAssignMemoryRegion;
    GetMemoryRegionInfo getMemoryRegionInfo;
    DestroyMemoryRegion destroyMemoryRegion;
    RegisterChannelHandler registerChannelHandler;
    RegisterOpHandler registerOpHandler;
    RegisterIdleHandler registerIdleHandler;
    RegisterAllocateHandler registerAllocateHandler;
    RegisterFreeHandler registerFreeHandler;
    RegisterRndvOpHandler registerRndvOpHandler;
    RegisterTLSCb registerTLSCb;
    MemoryAllocatorCreate memoryAllocatorCreate;
    MemoryAllocatorDestroy memoryAllocatorDestroy;
    MemoryAllocatorSetMrKey memoryAllocatorSetMrKey;
    MemoryAllocatorMemOffset memoryAllocatorMemOffset;
    MemoryAllocatorFreeSize memoryAllocatorFreeSize;
    MemoryAllocatorAllocate memoryAllocatorAllocate;
    MemoryAllocatorFree memoryAllocatorFree;
};

void *g_hcomDl = nullptr;
static HcomFunc g_hcomFunc;

static int HcomDlsymServiceLayerServiceHandlerFunc(void)
{
    int ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_GetChannel", reinterpret_cast<void **>(&g_hcomFunc.getChannel));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_GetContextType", reinterpret_cast<void **>(&g_hcomFunc.getContextType));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_GetResult", reinterpret_cast<void **>(&g_hcomFunc.getResult));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_GetRspCtx", reinterpret_cast<void **>(&g_hcomFunc.getRspCtx));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_GetOpInfo", reinterpret_cast<void **>(&g_hcomFunc.getOpInfo));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_GetMessageData", reinterpret_cast<void **>(&g_hcomFunc.getMessageData));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_GetMessageDataLen", reinterpret_cast<void **>(&g_hcomFunc.getMessageDataLen));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static int HcomDlsymServiceLayerServiceRndvHandlerFunc(void)
{
    int ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_GetRndvChannel", reinterpret_cast<void **>(&g_hcomFunc.getRndvChannel));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_GetRndvCtxType", reinterpret_cast<void **>(&g_hcomFunc.getRndvCtxType));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_GetRndvOpInfo", reinterpret_cast<void **>(&g_hcomFunc.getRndvOpInfo));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_GetRndvMessage", reinterpret_cast<void **>(&g_hcomFunc.getRndvMessage));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_RndvFreeContext", reinterpret_cast<void **>(&g_hcomFunc.rndvFreeContext));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_RndvReply", reinterpret_cast<void **>(&g_hcomFunc.rndvReply));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static int HcomDlsymServiceLayerRndvFunc(void)
{
    int ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_SyncRndvCall", reinterpret_cast<void **>(&g_hcomFunc.syncRndvCall));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_AsyncRndvCall", reinterpret_cast<void **>(&g_hcomFunc.asyncRndvCall));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_SyncRndvSglCall", reinterpret_cast<void **>(&g_hcomFunc.syncRndvSglCall));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_AsyncRndvSglCall", reinterpret_cast<void **>(&g_hcomFunc.asyncRndvSglCall));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static int HcomDlsymServiceLayerSendFunc(void)
{
    int ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_SetUpCtx", reinterpret_cast<void **>(&g_hcomFunc.setUpCtx));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_GetUpCtx", reinterpret_cast<void **>(&g_hcomFunc.getUpCtx));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_PostSend", reinterpret_cast<void **>(&g_hcomFunc.postSend));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_PostResponse", reinterpret_cast<void **>(&g_hcomFunc.postResponse));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_PostSendRaw", reinterpret_cast<void **>(&g_hcomFunc.postSendRaw));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_PostResponseRaw", reinterpret_cast<void **>(&g_hcomFunc.postResponseRaw));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_PostSendRawSgl", reinterpret_cast<void **>(&g_hcomFunc.postSendRawSgl));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static int HcomDlsymServiceLayerCallFunc(void)
{
    int ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_SyncCall", reinterpret_cast<void **>(&g_hcomFunc.syncCall));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_AsyncCall", reinterpret_cast<void **>(&g_hcomFunc.asyncCall));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_SyncCallRaw", reinterpret_cast<void **>(&g_hcomFunc.syncCallRaw));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_AsyncCallRaw", reinterpret_cast<void **>(&g_hcomFunc.asyncCallRaw));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_SyncCallRawSgl", reinterpret_cast<void **>(&g_hcomFunc.syncCallRawSgl));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_AsyncCallRawSgl", reinterpret_cast<void **>(&g_hcomFunc.asyncCallRawSgl));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static int HcomDlsymServiceLayerOnesideFunc(void)
{
    int ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_RegisterMemoryRegion", reinterpret_cast<void **>(&g_hcomFunc.registerMemoryRegion));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_RegisterAssignMemoryRegion", reinterpret_cast<void **>(&g_hcomFunc.registerAssignMemoryRegion));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_GetMemoryRegionInfo", reinterpret_cast<void **>(&g_hcomFunc.getMemoryRegionInfo));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_DestroyMemoryRegion", reinterpret_cast<void **>(&g_hcomFunc.destroyMemoryRegion));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_Read", reinterpret_cast<void **>(&g_hcomFunc.read));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_ReadSgl", reinterpret_cast<void **>(&g_hcomFunc.readSgl));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_Write", reinterpret_cast<void **>(&g_hcomFunc.write));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_WriteSgl", reinterpret_cast<void **>(&g_hcomFunc.writeSgl));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static int HcomDlsymServiceLayerChannelHandleFunc(void)
{
    int ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_Close", reinterpret_cast<void **>(&g_hcomFunc.channelClose));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_DeRefer", reinterpret_cast<void **>(&g_hcomFunc.channelDeRefer));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_SetOneSideTimeout", reinterpret_cast<void **>(&g_hcomFunc.setOneSideTimeout));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_SetTwoSideTimeout", reinterpret_cast<void **>(&g_hcomFunc.setTwoSideTimeout));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static int HcomDlsymServiceLayerServiceInitFunc(void)
{
    int ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_SetExternalLogger", reinterpret_cast<void **>(&g_hcomFunc.setExternalLogger));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_Create", reinterpret_cast<void **>(&g_hcomFunc.serviceCreate));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_SetOobIpAndPort", reinterpret_cast<void **>(&g_hcomFunc.setOobIpAndPort));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_GetOobIpAndPort", reinterpret_cast<void **>(&g_hcomFunc.getOobIpAndPort));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_Start", reinterpret_cast<void **>(&g_hcomFunc.serviceStart));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_Stop", reinterpret_cast<void **>(&g_hcomFunc.serviceStop));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_Destroy", reinterpret_cast<void **>(&g_hcomFunc.serviceDestroy));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_Connect", reinterpret_cast<void **>(&g_hcomFunc.channelConnect));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Channel_Destroy", reinterpret_cast<void **>(&g_hcomFunc.channelDestroy));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static int HcomDlsymServiceLayerServiceCreateRegisterFunc(void)
{
    int ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_RegisterChannelHandler", reinterpret_cast<void **>(&g_hcomFunc.registerChannelHandler));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_RegisterOpHandler", reinterpret_cast<void **>(&g_hcomFunc.registerOpHandler));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_RegisterIdleHandler", reinterpret_cast<void **>(&g_hcomFunc.registerIdleHandler));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_RegisterAllocateHandler", reinterpret_cast<void **>(&g_hcomFunc.registerAllocateHandler));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_RegisterFreeHandler", reinterpret_cast<void **>(&g_hcomFunc.registerFreeHandler));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_RegisterRndvOpHandler", reinterpret_cast<void **>(&g_hcomFunc.registerRndvOpHandler));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Service_RegisterTLSCb", reinterpret_cast<void **>(&g_hcomFunc.registerTLSCb));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static int HcomDlsymTransLayerAllocatorFunc(void)
{
    int ret = Hcom4dbLoadSymbol(g_hcomDl, "Net_MemoryAllocatorCreate", reinterpret_cast<void **>(&g_hcomFunc.memoryAllocatorCreate));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Net_MemoryAllocatorDestroy", reinterpret_cast<void **>(&g_hcomFunc.memoryAllocatorDestroy));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Net_MemoryAllocatorSetMrKey", reinterpret_cast<void **>(&g_hcomFunc.memoryAllocatorSetMrKey));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Net_MemoryAllocatorMemOffset", reinterpret_cast<void **>(&g_hcomFunc.memoryAllocatorMemOffset));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Net_MemoryAllocatorAllocate", reinterpret_cast<void **>(&g_hcomFunc.memoryAllocatorAllocate));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Net_MemoryAllocatorFreeSize", reinterpret_cast<void **>(&g_hcomFunc.memoryAllocatorFreeSize));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = Hcom4dbLoadSymbol(g_hcomDl, "Net_MemoryAllocatorFree", reinterpret_cast<void **>(&g_hcomFunc.memoryAllocatorFree));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

void FinishOckRpcDl(void)
{
    if (g_hcomDl != nullptr) {
        Hcom4dbCloseDl(g_hcomDl);
        g_hcomDl = nullptr;
    }

    if (memset_sp(&g_hcomFunc, sizeof(g_hcomFunc), 0 ,sizeof(g_hcomFunc)) != EOK) {
        OCK_RPC_LOG_ERROR("memset_sp failed");
    }
}

using DlSymbolFunction = int (*)(void);
using DlSymFuncs = struct {
    std::string name;
    DlSymbolFunction func;
};
namespace {
    constexpr uint16_t DL_SYMBOL_FUNC_ARR_LEN = 10;
}
static DlSymFuncs g_dlSymFuncsArr[DL_SYMBOL_FUNC_ARR_LEN] = {
    {"HcomDlsymServiceLayerCallFunc", HcomDlsymServiceLayerCallFunc},
    {"HcomDlsymServiceLayerChannelHandleFunc", HcomDlsymServiceLayerChannelHandleFunc},
    {"HcomDlsymServiceLayerOnesideFunc", HcomDlsymServiceLayerOnesideFunc},
    {"HcomDlsymServiceLayerRndvFunc", HcomDlsymServiceLayerRndvFunc},
    {"HcomDlsymServiceLayerSendFunc", HcomDlsymServiceLayerSendFunc},
    {"HcomDlsymServiceLayerServiceCreateRegisterFunc", HcomDlsymServiceLayerServiceCreateRegisterFunc},
    {"HcomDlsymServiceLayerServiceHandlerFunc", HcomDlsymServiceLayerServiceHandlerFunc},
    {"HcomDlsymServiceLayerServiceInitFunc", HcomDlsymServiceLayerServiceInitFunc},
    {"HcomDlsymServiceLayerServiceRndvHandlerFunc", HcomDlsymServiceLayerServiceRndvHandlerFunc},
    {"HcomDlsymTransLayerAllocatorFunc", HcomDlsymTransLayerAllocatorFunc},
};

static int HcomDlsym(void)
{
    int ret = OCK_RPC_OK;
    for (uint16_t i = 0; i < DL_SYMBOL_FUNC_ARR_LEN; i++) {
        ret = g_dlSymFuncsArr[i].func();
        if (ret != OCK_RPC_OK) {
            OCK_RPC_LOG_ERROR("HCOM dlsymbol function:" << g_dlSymFuncsArr[i].name << " failed.");
        }
    }
    return OCK_RPC_OK;
}

int InitOckRpcDl(char *path, uint32_t pathLen)
{
    if (path == nullptr || pathLen == 0) {
        OCK_RPC_LOG_ERROR("Dlopen hcom path is nullptr");
        return OCK_RPC_ERR;
    }

    int ret = OCK_RPC_OK;
    if (g_hcomDl != nullptr) {
        return OCK_RPC_OK;
    }

    ret = Hcom4dbOpenDl(&g_hcomDl, path);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Dlopen hcom path " << path);
        return OCK_RPC_ERR;
    }

    ret = HcomDlsym();
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Dlsym hcom server func, path");
        FinishOckRpcDl();
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

int Service_GetChannel(Service_Context context, Net_Channel *channel)
{
    int ret = 0;
    if (g_hcomFunc.getChannel != nullptr) {
        ret = g_hcomFunc.getChannel(context, channel);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Service_GetContextType(Service_Context context, Service_ContextType *type)
{
    int ret = 0;
    if (g_hcomFunc.getContextType != nullptr) {
        ret = g_hcomFunc.getContextType(context, type);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Service_GetResult(Service_Context context, int *result)
{
    int ret = 0;
    if (g_hcomFunc.getResult != nullptr) {
        ret = g_hcomFunc.getResult(context, result);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Service_GetRspCtx(Service_Context context, Service_RspCtx *rsp)
{
    int ret = 0;
    if (g_hcomFunc.getRspCtx != nullptr) {
        ret = g_hcomFunc.getRspCtx(context, rsp);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Service_GetOpInfo(Service_Context context, Service_OpInfo *info)
{
    int ret = 0;
    if (g_hcomFunc.getOpInfo != nullptr) {
        ret = g_hcomFunc.getOpInfo(context, info);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

void *Service_GetMessageData(Service_Context context)
{
    void *ret = nullptr;
    if (g_hcomFunc.getMessageData != nullptr) {
        ret = g_hcomFunc.getMessageData(context);
    }

    return ret;
}

uint32_t Service_GetMessageDataLen(Service_Context context)
{
    uint32_t ret = 0;
    if (g_hcomFunc.getMessageDataLen != nullptr) {
        ret = g_hcomFunc.getMessageDataLen(context);
    }

    return ret;
}

int Service_GetRndvChannel(Service_RndvContext context, Net_Channel *channel)
{
    int ret = 0;
    if (g_hcomFunc.getRndvChannel != nullptr) {
        ret = g_hcomFunc.getRndvChannel(context, channel);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Service_GetRndvCtxType(Service_RndvContext context, Service_RndvType *type)
{
    int ret = 0;
    if (g_hcomFunc.getRndvCtxType != nullptr) {
        ret = g_hcomFunc.getRndvCtxType(context, type);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Service_GetRndvOpInfo(Service_RndvContext context, Service_OpInfo *info)
{
    int ret = 0;
    if (g_hcomFunc.getRndvOpInfo != nullptr) {
        ret = g_hcomFunc.getRndvOpInfo(context, info);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Service_GetRndvMessage(Service_RndvContext context, Service_RndvMessage *message)
{
    int ret = 0;
    if (g_hcomFunc.getRndvMessage != nullptr) {
        ret = g_hcomFunc.getRndvMessage(context, message);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Service_RndvFreeContext(Service_RndvContext context)
{
    int ret = 0;
    if (g_hcomFunc.rndvFreeContext != nullptr) {
        ret = g_hcomFunc.rndvFreeContext(context);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Service_RndvReply(Service_RndvContext context, Service_OpInfo *opInfo, Service_Message *message,
    Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.rndvReply != nullptr) {
        ret = g_hcomFunc.rndvReply(context, opInfo, message, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_SetUpCtx(Net_Channel channel, uint64_t ctx)
{
    int ret = 0;
    if (g_hcomFunc.setUpCtx != nullptr) {
        ret = g_hcomFunc.setUpCtx(channel, ctx);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_GetUpCtx(Net_Channel channel, uint64_t *ctx)
{
    int ret = 0;
    if (g_hcomFunc.getUpCtx != nullptr) {
        ret = g_hcomFunc.getUpCtx(channel, ctx);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_PostSend(Net_Channel channel, Service_OpInfo *opInfo, Service_Message *message, Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.postSend != nullptr) {
        ret = g_hcomFunc.postSend(channel, opInfo, message, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_PostResponse(Net_Channel channel, Service_RspCtx rspCtx, Service_OpInfo *opInfo, Service_Message *message,
    Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.postResponse != nullptr) {
        ret = g_hcomFunc.postResponse(channel, rspCtx, opInfo, message, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_PostSendRaw(Net_Channel channel, Service_Message *message, Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.postSendRaw != nullptr) {
        ret = g_hcomFunc.postSendRaw(channel, message, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_PostResponseRaw(Net_Channel channel, Service_RspCtx rspCtx, Service_Message *message, Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.postResponseRaw != nullptr) {
        ret = g_hcomFunc.postResponseRaw(channel, rspCtx, message, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_PostSendRawSgl(Net_Channel channel, Service_SglRequest *message, Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.postSendRawSgl != nullptr) {
        ret = g_hcomFunc.postSendRawSgl(channel, message, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_PostResponseRawSgl(Net_Channel channel, Service_RspCtx rspCtx, Service_SglRequest *message,
    Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.postResponseRawSgl != nullptr) {
        ret = g_hcomFunc.postResponseRawSgl(channel, rspCtx, message, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_SyncCall(Net_Channel channel, Service_OpInfo *reqInfo, Service_Message *req, Service_OpInfo *rspInfo,
    Service_Message *rsp)
{
    int ret = 0;
    if (g_hcomFunc.syncCall != nullptr) {
        ret = g_hcomFunc.syncCall(channel, reqInfo, req, rspInfo, rsp);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_AsyncCall(Net_Channel channel, Service_OpInfo *opInfo, Service_Message *req, Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.asyncCall != nullptr) {
        ret = g_hcomFunc.asyncCall(channel, opInfo, req, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_SyncCallRaw(Net_Channel channel, Service_Message *req, Service_Message *rsp)
{
    int ret = 0;
    if (g_hcomFunc.syncCallRaw != nullptr) {
        ret = g_hcomFunc.syncCallRaw(channel, req, rsp);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_AsyncCallRaw(Net_Channel channel, Service_Message *req, Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.asyncCallRaw != nullptr) {
        ret = g_hcomFunc.asyncCallRaw(channel, req, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_SyncCallRawSgl(Net_Channel channel, Service_SglRequest *req, Service_Message *rsp)
{
    int ret = 0;
    if (g_hcomFunc.syncCallRawSgl != nullptr) {
        ret = g_hcomFunc.syncCallRawSgl(channel, req, rsp);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_AsyncCallRawSgl(Net_Channel channel, Service_SglRequest *req, Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.asyncCallRawSgl != nullptr) {
        ret = g_hcomFunc.asyncCallRawSgl(channel, req, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_SyncRndvCall(Net_Channel channel, Service_OpInfo *reqInfo, Service_Request *req, Service_OpInfo *rspInfo,
    Service_Message *rsp)
{
    int ret = 0;
    if (g_hcomFunc.syncRndvCall != nullptr) {
        ret = g_hcomFunc.syncRndvCall(channel, reqInfo, req, rspInfo, rsp);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_AsyncRndvCall(Net_Channel channel, Service_OpInfo *reqInfo, Service_Request *req, Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.asyncRndvCall != nullptr) {
        ret = g_hcomFunc.asyncRndvCall(channel, reqInfo, req, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_SyncRndvSglCall(Net_Channel channel, Service_OpInfo *reqInfo, Service_SglRequest *req,
    Service_OpInfo *rspInfo, Service_Message *rsp)
{
    int ret = 0;
    if (g_hcomFunc.syncRndvSglCall != nullptr) {
        ret = g_hcomFunc.syncRndvSglCall(channel, reqInfo, req, rspInfo, rsp);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_AsyncRndvSglCall(Net_Channel channel, Service_OpInfo *reqInfo, Service_SglRequest *req,
    Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.asyncRndvSglCall != nullptr) {
        ret = g_hcomFunc.asyncRndvSglCall(channel, reqInfo, req, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_Read(Net_Channel channel, Service_Request *req, Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.read != nullptr) {
        ret = g_hcomFunc.read(channel, req, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_ReadSgl(Net_Channel channel, Service_SglRequest *req, Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.readSgl != nullptr) {
        ret = g_hcomFunc.readSgl(channel, req, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_Write(Net_Channel channel, Service_Request *req, Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.write != nullptr) {
        ret = g_hcomFunc.write(channel, req, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Channel_WriteSgl(Net_Channel channel, Service_SglRequest *req, Channel_Callback *cb)
{
    int ret = 0;
    if (g_hcomFunc.writeSgl != nullptr) {
        ret = g_hcomFunc.writeSgl(channel, req, cb);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

void Channel_Close(Net_Channel channel)
{
    if (g_hcomFunc.channelClose != nullptr) {
        g_hcomFunc.channelClose(channel);
    }

    return;
}

void Channel_SetOneSideTimeout(Net_Channel channel, int32_t timeout)
{
    if (g_hcomFunc.setOneSideTimeout != nullptr) {
        g_hcomFunc.setOneSideTimeout(channel, timeout);
    }

    return;
}

void Channel_SetTwoSideTimeout(Net_Channel channel, int32_t timeout)
{
    if (g_hcomFunc.setTwoSideTimeout != nullptr) {
        g_hcomFunc.setTwoSideTimeout(channel, timeout);
    }

    return;
}

void Service_SetExternalLogger(Service_LogHandler h)
{
    if (g_hcomFunc.setExternalLogger != nullptr) {
        g_hcomFunc.setExternalLogger(h);
    }

    return;
}

int Service_Create(Service_Type t, const char *name, uint8_t startOobSvr, Net_Service *service)
{
    int ret = 0;
    if (g_hcomFunc.serviceCreate != nullptr) {
        ret = g_hcomFunc.serviceCreate(t, name, startOobSvr, service);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

void Service_SetOobIpAndPort(Net_Service service, const char *ip, uint16_t port)
{
    if (g_hcomFunc.setOobIpAndPort != nullptr) {
        g_hcomFunc.setOobIpAndPort(service, ip, port);
    }

    return;
}

bool Service_GetOobIpAndPort(Net_Service service, char ***ipArray, uint16_t **portArray, int *length)
{
    bool ret = false;
    if (g_hcomFunc.getOobIpAndPort != nullptr) {
        ret = g_hcomFunc.getOobIpAndPort(service, ipArray, portArray, length);
    } else {
        ret = false;
    }
    return ret;
}

int Service_Start(Net_Service service, Service_Options options)
{
    int ret = 0;
    if (g_hcomFunc.serviceStart != nullptr) {
        ret = g_hcomFunc.serviceStart(service, options);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

void Service_Stop(Net_Service service)
{
    if (g_hcomFunc.serviceStop != nullptr) {
        g_hcomFunc.serviceStop(service);
    }

    return;
}

int Service_Destroy(Net_Service service)
{
    int ret = 0;
    if (g_hcomFunc.serviceDestroy != nullptr) {
        ret = g_hcomFunc.serviceDestroy(service);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Service_Connect(Net_Service service, const char *oobIpOrName, uint16_t oobPort, const char *payload,
    Net_Channel *channel, Service_ConnectOpt *options)
{
    int ret = 0;
    if (g_hcomFunc.channelConnect != nullptr) {
        ret = g_hcomFunc.channelConnect(service, oobIpOrName, oobPort, payload, channel, options);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

void Channel_DeRefer(Net_Channel channel)
{
    if (g_hcomFunc.channelDeRefer != nullptr) {
        g_hcomFunc.channelDeRefer(channel);
    }

    return;
}

void Channel_Destroy(Net_Channel channel)
{
    if (g_hcomFunc.channelDestroy != nullptr) {
        g_hcomFunc.channelDestroy(channel);
    }

    return;
}

int Service_RegisterMemoryRegion(Net_Service service, uint64_t size, Service_MemoryRegion *mr)
{
    int ret = 0;
    if (g_hcomFunc.registerMemoryRegion != nullptr) {
        ret = g_hcomFunc.registerMemoryRegion(service, size, mr);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Service_RegisterAssignMemoryRegion(Net_Service service, uintptr_t address, uint64_t size, Service_MemoryRegion *mr)
{
    int ret = 0;
    if (g_hcomFunc.registerAssignMemoryRegion != nullptr) {
        ret = g_hcomFunc.registerAssignMemoryRegion(service, address, size, mr);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Service_GetMemoryRegionInfo(Service_MemoryRegion mr, Service_MemoryRegionInfo *info)
{
    int ret = 0;
    if (g_hcomFunc.getMemoryRegionInfo != nullptr) {
        ret = g_hcomFunc.getMemoryRegionInfo(mr, info);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

void Service_DestroyMemoryRegion(Net_Service service, Service_MemoryRegion mr)
{
    if (g_hcomFunc.destroyMemoryRegion != nullptr) {
        g_hcomFunc.destroyMemoryRegion(service, mr);
    }

    return;
}

uintptr_t Service_RegisterChannelHandler(Net_Service service, Service_HandlerType t, Service_ChannelHandler h,
    Service_ChannelPolicy policy, uint64_t usrCtx)
{
    uintptr_t ret = 0;
    if (g_hcomFunc.registerChannelHandler != nullptr) {
        ret = g_hcomFunc.registerChannelHandler(service, t, h, policy, usrCtx);
    }
    return ret;
}


uintptr_t Service_RegisterOpHandler(Net_Service service, uint16_t i, Service_OpHandlerType t, Service_RequestHandler h,
    uint64_t usrCtx)
{
    uintptr_t ret = 0;
    if (g_hcomFunc.registerOpHandler != nullptr) {
        ret = g_hcomFunc.registerOpHandler(service, i, t, h, usrCtx);
    }
    return ret;
}

uintptr_t Service_RegisterIdleHandler(Net_Service service, Service_IdleHandler h, uint64_t usrCtx)
{
    uintptr_t ret = 0;
    if (g_hcomFunc.registerIdleHandler != nullptr) {
        ret = g_hcomFunc.registerIdleHandler(service, h, usrCtx);
    }
    return ret;
}

uintptr_t Service_RegisterAllocateHandler(Net_Service service, Service_RndvMemAllocate h)
{
    uintptr_t ret = 0;
    if (g_hcomFunc.registerAllocateHandler != nullptr) {
        ret = g_hcomFunc.registerAllocateHandler(service, h);
    }
    return ret;
}

uintptr_t Service_RegisterFreeHandler(Net_Service service, Service_RndvMemFree h)
{
    uintptr_t ret = 0;
    if (g_hcomFunc.registerFreeHandler != nullptr) {
        ret = g_hcomFunc.registerFreeHandler(service, h);
    }
    return ret;
}

uintptr_t Service_RegisterRndvOpHandler(Net_Service service, Service_RndvHandler h)
{
    uintptr_t ret = 0;
    if (g_hcomFunc.registerRndvOpHandler != nullptr) {
        ret = g_hcomFunc.registerRndvOpHandler(service, h);
    }
    return ret;
}

uintptr_t Service_RegisterTLSCb(Net_Service service, Net_TlsGetCertCb certCb, Net_TlsGetPrivateKeyCb priKeyCb,
    Net_TlsGetCACb caCb)
{
    uintptr_t ret = 0;
    if (g_hcomFunc.registerTLSCb != nullptr) {
        ret = g_hcomFunc.registerTLSCb(service, certCb, priKeyCb, caCb);
    }
    return ret;
}

int Net_MemoryAllocatorCreate(
    Net_MemoryAllocatorType t, Net_MemoryAllocatorOptions *options, Net_MemoryAllocator *allocator)
{
    int ret = 0;
    if (g_hcomFunc.memoryAllocatorCreate != nullptr) {
        ret = g_hcomFunc.memoryAllocatorCreate(t, options, allocator);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Net_MemoryAllocatorDestroy(Net_MemoryAllocator allocator)
{
    int ret = 0;
    if (g_hcomFunc.memoryAllocatorDestroy != nullptr) {
        ret = g_hcomFunc.memoryAllocatorDestroy(allocator);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Net_MemoryAllocatorSetMrKey(Net_MemoryAllocator allocator, uint32_t mrKey)
{
    int ret = 0;
    if (g_hcomFunc.memoryAllocatorSetMrKey != nullptr) {
        ret = g_hcomFunc.memoryAllocatorSetMrKey(allocator, mrKey);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Net_MemoryAllocatorMemOffset(Net_MemoryAllocator allocator, uintptr_t address, uintptr_t *offset)
{
    int ret = 0;
    if (g_hcomFunc.memoryAllocatorMemOffset != nullptr) {
        ret = g_hcomFunc.memoryAllocatorMemOffset(allocator, address, offset);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Net_MemoryAllocatorFreeSize(Net_MemoryAllocator allocator, uintptr_t *size)
{
    int ret = 0;
    if (g_hcomFunc.memoryAllocatorFreeSize != nullptr) {
        ret = g_hcomFunc.memoryAllocatorFreeSize(allocator, size);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Net_MemoryAllocatorAllocate(Net_MemoryAllocator allocator, uint64_t size, uintptr_t *address, uint32_t *key)
{
    int ret = 0;
    if (g_hcomFunc.memoryAllocatorAllocate != nullptr) {
        ret = g_hcomFunc.memoryAllocatorAllocate(allocator, size, address, key);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}

int Net_MemoryAllocatorFree(Net_MemoryAllocator allocator, uintptr_t address)
{
    int ret = 0;
    if (g_hcomFunc.memoryAllocatorFree != nullptr) {
        ret = g_hcomFunc.memoryAllocatorFree(allocator, address);
    } else {
        ret = OCK_RPC_ERR;
    }
    return ret;
}