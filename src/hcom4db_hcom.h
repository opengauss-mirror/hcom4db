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
 * hcom4db_hcom.h
 *
 *
 * IDENTIFICATION
 *    src/hcom4db_hcom.h
 *
 * -------------------------------------------------------------------------
 */

#ifndef HCOM4DB_HCOM_H
#define HCOM4DB_HCOM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_16 16
#define NUM_64 64
#define NUM_96 96
#define NUM_128 128
#define NUM_256 256
#define NUM_512 512
#define NUM_1024 1024

#define NET_SGE_MAX_IOV 4
#define SER_C_FLAGS_BIT(i) (1UL << (i))
#define SER_C_CHANNEL_SELF_POLLING SER_C_FLAGS_BIT(0)
#define SER_C_CHANNEL_EVENT_POLLING SER_C_FLAGS_BIT(1)

/**
 * @brief service, which include oob & multi protocols(TCP/RDMA/SHM) workers & callback etc
 */
typedef uintptr_t Net_Service;

/**
 * @brief Channel, represent multi connections(EPs) of one protocol
 *
 * two side opreation, Net_ChannelSend
 * read operation from remote, Net_ChannelRead
 * write operation from remote, Net_ChannelWrite
 */
typedef uintptr_t Net_Channel;

/**
 * @brief MemoryRegion, which region memory in RDMA Nic for write/read operation
 */
typedef uintptr_t Service_MemoryRegion;

/**
 * @brief Read/write mr info for one side rdma operation
 */
typedef struct {
    uintptr_t lAddress; // local memory region address
    uint32_t lKey;      // local memory region key
    uint32_t size;      // data size
    void *tseg;
} Service_MemoryRegionInfo;

/**
 * @brief service rpc message
 */
typedef struct {
    void *data;     // buffer address, user can free it after invoke api
    uint32_t size;  // buffer size
} __attribute__((packed)) Service_Message;

/**
 * @brief service rdma request
 */
typedef struct {
    uintptr_t lAddress; // local buffer address, user can free it after invoke callback
    uintptr_t rAddress; // remote buffer address, user can free it after invoke callback
    uint32_t lKey;      // local memory region key, for rdma etc.
    uint32_t rKey;      // remote memory region key, for rdma etc.
    uint32_t size;      // buffer size
    unsigned long memid;
    void *srcSeg;
    void *dstSeg;
} __attribute__((packed)) Service_Request;

typedef struct {
    Service_Request *iov; // sgl array
    uint16_t iovCount;    // max count:NUM_16
} __attribute__((packed)) Service_SglRequest;

typedef struct {
    uint16_t opCode;    // operation code, 0~999
    int16_t timeout;    // timeout in seconds
    uint8_t flags;      // flags in user case
    int16_t errorCode;  // error code
} Service_OpInfo;

/**
 * @brief channel callback type
 */
typedef enum {
    C_CHANNEL_FUNC_CB = 0,      // use channel function param (const NetCallBack *cb)
    C_CHANNEL_GLOBAL_CB = 1,    // use service RegisterOpHandler
} Channel_CBType;

/**
 * @brief Service context. which used for callback as param
 */
typedef uintptr_t Service_Context;

/**
 * @brief Service context, which used for callback as param
 */
typedef uintptr_t Service_RspCtx;

/**
 * @brief Callback function which will be invoked by async use mode
 */

typedef void (*Channel_CallbackFunc)(void *arg, Service_Context context);

/**
 * @brief RPC call completion handle.
 *
 * This structure should be allocated by the user and can be passed to communication
 * primitives, such as Net_ChannelSend/Net_ChannelCall. When the structure object is passed
 * in, the communication routine changes to asynchrounous mode. And if the routine
 * return success, the actual completion result will be notified through this callback.
 */
typedef struct {
    Channel_CallbackFunc cb;    // User callback function
    void *arg;                  // Argument of callback
} Channel_Callback;

/**
 * @brief Context type, part if Service_Context, sync mode is not aware most of them
 */
typedef enum {
    SERVICE_RECEIVED = 0,     /* support invoke all functions */
    SERVICE_RECEIVED_RAW = 1, /* support invoke most functions except Service_GetOpInfo() */
    SERVICE_SENT = 2,        /* support invoke basic functions except Service_GetMessage() and Service_GetRspCtx() */
    SERVICE_SENT_RAW = 3,    /* support invoke basic functions except Service_GetMessage(), Service_GetRspCtx() and Service_GetInfo() */
    SERVICE_ONE_SIDE = 4,    /* support invoke basic functions except Service_GetMessage(), Service_GetRspCtx() and Service_GetInfo() */
    SERVICE_INVALID_OP_TYPE = 255,
} Service_ContextType;

/**
 * @brief Get the channel
 *
 * @return 0 means OK, other value means failed
 */
int Service_GetChannel(Service_Context context, Net_Channel *channel);

/**
 * @brief Get the context type
 *
 * @return 0 means OK, other value means failed
 */
int Service_GetContextType(Service_Context context, Service_ContextType *type);

/**
 * @brief Get the result
 *
 * @return 0 means OK, other value means failed
 */
int Service_GetResult(Service_Context context, int *result);

/**
 * @brief Get response context for send rsp message
 *
 * @note only support SERVICE_RECEIVE/SERVICE_RECEIVE_RWA type invoke
 *
 * @return 0 means OK, other value means failed
 */
int Service_GetRspCtx(Service_Context context, Service_RspCtx *rsp);

/**
 * @brief Get op info by user input
 *
 * @note only support SERVICE_SEND/SERVICE_RECEIVE type invoke
 *
 * @return 0 means OK, other value means failed
 */
int Service_GetOpInfo(Service_Context context, Service_OpInfo *info);

/**
 * @brief Get message data received which valid in callback lifetime
 *
 * @note Only support SERVICE_RECEIVE/SERVICE_RECEIVE_RWA type invoke
 * @note If user want to use message in other thread, need to copy data
 *
 * @return valid address if success, NULL if failed
 */
void *Service_GetMessageData(Service_Context context);

/**
 * @brief Get message data len received which valid in callback lifetime
 *
 * @note Only support SERVICE_RECEIVE/SERVICE_RECEIVE_RWA type invoke
 * @note If user want to use message in other thread, need to copy data
 *
 * @return valid length if success, 0 if failed
 */
uint32_t Service_GetMessageDataLen(Service_Context context);

/**
 * @brief Clone service context
 *
 * @return valid address if success, 0 if failed
 */
Service_Context Service_ContextClone(Service_Context context);

/**
 * @brief Destroy context by cloned
 */
void Service_ContextDeClone(Service_Context context);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param context       [in] service context
 * @param opInfo        [in] get opInfo from Service_GetOpInfo
 * @param message       [in] user single common message
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the inerface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker therad.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in process.
 *
 * @note return ERROR, cb will not be invoked.
 */
int Service_ContextReply(Service_Context context, Service_OpInfo *opInfo, Service_Message *message,
                         Channel_Callback *cb);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param context       [in] service context
 * @param message       [in] user single common message
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the inerface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker therad.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in process.
 *
 * @note return ERROR, cb will not be invoked.
 */
int Service_ContextReplyRaw(Service_Context context, Service_Message *message, Channel_Callback *cb);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param context       [in] service context
 * @param message       [in] send to remote message which fill with local different MRs, send to the same remote MR by
 * loacal MRs sequence, you can free it after called, rKey/rAddress do not need to assign.
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the inerface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker therad.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in process.
 *
 * @note return ERROR, cb will not be invoked.
 */
int Service_ContextReplyRawSgl(Service_Context context, Service_SglRequest *message, Channel_Callback *cb);

/**
 * @brief Service rndv protocol context, which used for callback as param
 */
typedef uintptr_t Service_RndvContext;

typedef struct {
    uint16_t cnt;
    Service_Message msg[NET_SGE_MAX_IOV];
} Service_RndvMessage;

/**
 * @brief rndv context type
 */
typedef enum {
    C_SER_RNDV = 0,
    C_SER_RNDV_SGL = 1,
    C_SER_RNDV_INVALID_TYPE = 255,
} Service_RndvType;

/**
 * @brief Get the channel
 *
 * @return 0 means OK, other value means failed
 */
int Service_GetRndvChannel(Service_RndvContext context, Net_Channel *channel);

/**
 * @brief Get the context type
 *
 * @return 0 means OK, other value means failed
 */
int Service_GetRndvCtxType(Service_RndvContext context, Service_RndvType *type);

/**
 * @brief Get op info by user input
 *
 * @return 0 means OK, other value means failed
 */
int Service_GetRndvOpInfo(Service_RndvContext context, Service_OpInfo *info);

/**
 * @brief Get message data received which valid in callback lifetime
 *
 * @note If user want to use message in other thread, need to copy data
 *
 * @return 0 means OK, other value means failed
 */
int Service_GetRndvMessage(Service_Context context, Service_RndvMessage *message);

/**
 * @brief Free rndv context
 *
 * @return 0 means OK, other value means failed
 */
int Service_RndvFreeContext(Service_RndvContext context);

/**
 * @brief Reply message with op info to peer rndv msg
 *
 * @param context       [in] service context
 * @param opInfo        [in] get opInfo from Service_GetOpInfo
 * @param message       [in] user single common message
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the inerface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker therad.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in process.
 *
 * @note return ERROR, cb will not be invoked.
 */
int Service_RndvReply(Service_RndvContext context, Service_OpInfo *opInfo, Service_Message *message,
                      Channel_Callback *cb);

/**
 * @brief Set channel up context
 *
 * @return 0 means OK, other value means failed
 */
int Channel_SetUpCtx(Net_Channel channel, uint64_t ctx);

/**
 * @brief Get channel up context
 *
 * @return 0 means OK, other value means failed
 */
int Channel_GetUpCtx(Net_Channel channel, uint64_t *ctx);

/**
 * @brief Get channel type index
 *
 * @return 0 means OK, other value means failed
 */
int Channel_GetTypeIndex(Net_Channel channel, uint16_t *index);

/**
 * @brief Get channel callback type
 *
 * @return 0 means OK, other value means failed
 */
int Channel_GetCBType(Net_Channel channel, Channel_CBType *type);

/**
 * @brief Get channel peer ip and port
 *
 * @return 0 means OK, other value means failed
 */
int Channel_GetPeerIpPort(Net_Channel channel, char **ip, uint16_t *port);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param channel       [in] net channel
 * @param opInfo        [in] get opInfo from Service_GetOpInfo
 * @param message       [in] user single common message
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the inerface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker therad.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in process.
 *
 * @note return ERROR, cb will not be invoked.
 */
int Channel_PostSend(Net_Channel channel, Service_OpInfo *opInfo, Service_Message *message, Channel_Callback *cb);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param channel       [in] net channel
 * @param rspCtx        [in] get rspCtx from Service_GetRspCtx
 * @param opInfo        [in] get opInfo from Service_GetOpInfo
 * @param message       [in] user single common message
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the inerface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker therad.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in process.
 *
 * @note return ERROR, cb will not be invoked.
 */
int Channel_PostResponse(Net_Channel channel, Service_RspCtx rspCtx, Service_OpInfo *opInfo, Service_Message *message,
                         Channel_Callback *cb);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param channel       [in] net channel
 * @param message       [in] user single common message
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the inerface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker therad.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in process.
 *
 * @note return ERROR, cb will not be invoked.
 */
int Channel_PostSendRaw(Net_Channel channel, Service_Message *message, Channel_Callback *cb);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param channel       [in] net channel
 * @param rspCtx        [in] get rspCtx from Service_GetRspCtx
 * @param message       [in] user single common message
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the inerface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker therad.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in process.
 *
 * @note return ERROR, cb will not be invoked.
 */
int Channel_PostResponseRaw(Net_Channel channel, Service_RspCtx rspCtx, Service_Message *message, Channel_Callback *cb);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param channel       [in] net channel
 * @param message       [in] send to remote message which fill with local different MRs, send to the same remote MR by
 * loacal MRs sequence, you can free it after called, rKey/rAddress do not need to assign.
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the inerface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker therad.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in process.
 *
 * @note return ERROR, cb will not be invoked.
 */
int Channel_PostSendRawSgl(Net_Channel channel, Service_SglRequest *message, Channel_Callback *cb);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param channel       [in] net channel
 * @param rspCtx        [in] get rspCtx from Service_GetRspCtx
 * @param message       [in] send to remote message which fill with local different MRs, send to the same remote MR by
 * loacal MRs sequence, you can free it after called, rKey/rAddress do not need to assign.
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the inerface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker therad.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in process.
 *
 * @note return ERROR, cb will not be invoked.
 */
int Channel_PostResponseRawSgl(Net_Channel channel, Service_RspCtx rspCtx, Service_SglRequest *message,
                               Channel_Callback *cb);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param channel       [in] net channel
 * @param reqInfo       [in] peer can get opInfo from Service_GetOpInfo
 * @param req           [in] send to remote message.
 * @param rspInfo       [in] peer response info
 * @param rsp           [in] receive remote response message. If @b response is not null, it will receive a
 * response. If user does not know the real length of response, user can set @b response->data to null, so
 * RPC will allocate enough memory to hold the response, and store the memory address to @b reponse->data. User are
 * required to use free() to release this memory. There will be additional overhead.
 *
 * @return 0 means OK, other value means failed.
 */
int Channel_SyncCall(Net_Channel channel, Service_OpInfo *reqInfo, Service_Message *req, Service_OpInfo *rspInfo,
                     Service_Message *rsp);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param channel       [in] net channel
 * @param opInfo        [in] get opInfo from Service_GetOpInfo
 * @param message       [in] send to remote message.
 * @param cb            [in] receive message will store in NetServiceContext::Message() in callback lifetime. If
 * user want to use Message() in other thread, need to prepare buff and copy data out.
 *
 * @return 0 means the operation is in progress, cb will be invoked.
 *
 * @note return ERROR, cb will not be invoked.
 */
int Channel_AsyncCall(Net_Channel channel, Service_OpInfo *opInfo, Service_Message *req, Channel_Callback *cb);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param channel       [in] net channel
 * @param req           [in] send to remote message.
 * @param rsp           [in] receive remote response message. If @b response is not null, it will receive a
 * response. If user does not know the real length of response, user can set @b response->data to null, so
 * RPC will allocate enough memory to hold the response, and store the memory address to @b reponse->data. User are
 * required to use free() to release this memory. There will be additional overhead.
 *
 * @return 0 means OK, other value means failed.
 */
int Channel_SyncCallRaw(Net_Channel channel, Service_Message *req, Service_Message *rsp);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param channel       [in] net channel
 * @param message       [in] send to remote message.
 * @param cb            [in] receive message will store in NetServiceContext::Message() in callback lifetime. If
 * user want to use Message() in other thread, need to prepare buff and copy data out.
 *
 * @return 0 means the operation is in progress, cb will be invoked.
 *
 * @note return ERROR, cb will not be invoked.
 */
int Channel_AsyncCallRaw(Net_Channel channel, Service_Message *req, Channel_Callback *cb);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param channel       [in] net channel
 * @param req           [in] send to remote message which fill with local different MRs, send to the same remote MR by
 * loacal MRs sequence, you can free it after called, rKey/rAddress do not need to assign.
 * @param rsp           [in] receive remote response message. If @b response is not null, it will receive a
 * response. If user does not know the real length of response, user can set @b response->data to null, so
 * RPC will allocate enough memory to hold the response, and store the memory address to @b reponse->data. User are
 * required to use free() to release this memory. There will be additional overhead.
 *
 * @return 0 means OK, other value means failed.
 */
int Channel_SyncCallRawSgl(Net_Channel channel, Service_SglRequest *req, Service_Message *rsp);

/**
 * @brief Post send reponse message with op info to peer, and without waiting for response. Peer will be trigger
 * new request NetCallback also with op info
 *
 * @param channel       [in] net channel
 * @param message       [in] end to remote message which fill with local different MRs, send to the same remote MR by
 * loacal MRs sequence, you can free it after called, rKey/rAddress do not need to assign.
 * @param cb            [in] receive message will store in NetServiceContext::Message() in callback lifetime. If
 * user want to use Message() in other thread, need to prepare buff and copy data out.
 *
 * @return 0 means the operation is in progress, cb will be invoked.
 *
 * @note return ERROR, cb will not be invoked.
 */
int Channel_AsyncCallRawSgl(Net_Channel channel, Service_SglRequest *req, Channel_Callback *cb);

/**
 * @brief Post send a register MR with op info to peer, and waiting for response. Peer will be trigger new
 * request NetCallbcak also with op info. Poll mode:worker poll(default mode, support sync/async with sem), self
 * poll(support sync only)
 *
 * @param channel       [in] net channel
 * @param reqInfo       [in] peer can get opInfo from Service_GetOpInfo
 * @param req           [in] send local MR to remote for data message. rKey/rAddress do not need to assign
 * @param rspInfo       [in] peer response op info
 * @param rsp           [in] receive remote response message. If @b response is not null, it will receive a
 * response. If user does not know the real length of response, user can set @b response->data to null, so
 * RPC will allocate enough memory to hold the response, and store the memory address to @b reponse->data. User are
 * required to use free() to release this memory. There will be additional overhead.
 *
 * @return 0 means OK, other value means failed.
 *
 * @note Peer rsp message need invoke Send() with Service_OpInfo
 */
int Channel_SyncRndvCall(Net_Channel channel, Service_OpInfo *reqInfo, Service_Request *req, Service_OpInfo *rspInfo,
                         Service_Message *rsp);

/**
 * @brief Post send a register MR with op info to peer, and waiting for response. Peer will be trigger new
 * request NetCallbcak also with op info. Poll mode:worker poll(default mode, support sync/async with sem), self
 * poll(support sync only)
 *
 * @param channel       [in] net channel
 * @param reqInfo       [in] peer can get opInfo from Service_GetOpInfo
 * @param req           [in] send local MR to remote for data message. rKey/rAddress do not need to assign
 * @param cb            [in] receive message will store in NetServiceContext::Message() in callback lifetime. If
 * user want to use Message() in other thread, need to prepare buff and copy data out.
 *
 * @return 0 means the operation is in process, cb will be invoked.
 * Return ERROR, cb will not be invoked.
 *
 * @note Peer rsp message need invoke Send() with Service_OpInfo
 */
int Channel_AsyncRndvCall(Net_Channel channel, Service_OpInfo *reqInfo, Service_Request *req, Channel_Callback *cb);

/**
 * @brief Post send a register MR with op info to peer, and waiting for response. Peer will be trigger new
 * request NetCallbcak also with op info. Poll mode:worker poll(default mode, support sync/async with sem), self
 * poll(support sync only)
 *
 * @param channel       [in] net channel
 * @param reqInfo       [in] peer can get opInfo from Service_GetOpInfo
 * @param req           [in] send local MR to remote for data message. rKey/rAddress do not need to assign
 * @param rspInfo       [in] peer response op info
 * @param rsp           [in] receive remote response message. If @b response is not null, it will receive a
 * response. If user does not know the real length of response, user can set @b response->data to null, so
 * RPC will allocate enough memory to hold the response, and store the memory address to @b reponse->data. User are
 * required to use free() to release this memory. There will be additional overhead.
 *
 * @return 0 means OK, other value means failed.
 *
 * @note Peer rsp message need invoke Send() with Service_OpInfo
 */
int Channel_SyncRndvSglCall(Net_Channel channel, Service_OpInfo *reqInfo, Service_SglRequest *req,
                            Service_OpInfo *rspInfo, Service_Message *rsp);

/**
 * @brief Post send a register MR with op info to peer, and waiting for response. Peer will be trigger new
 * request NetCallbcak also with op info. Poll mode:worker poll(default mode, support sync/async with sem), self
 * poll(support sync only)
 *
 * @param channel       [in] net channel
 * @param reqInfo       [in] peer can get opInfo from Service_GetOpInfo
 * @param req           [in] send local MR to remote for data message. rKey/rAddress do not need to assign
 * @param cb            [in] receive message will store in NetServiceContext::Message() in callback lifetime. If
 * user want to use Message() in other thread, need to prepare buff and copy data out.
 *
 * @return 0 means the operation is in process, cb will be invoked.
 * Return ERROR, cb will not be invoked.
 *
 * @note Peer rsp message need invoke Send() with Service_OpInfo
 */
int Channel_AsyncRndvSglCall(Net_Channel channel, Service_OpInfo *reqInfo, Service_SglRequest *req,
                             Channel_Callback *cb);

/**
 * @brief Post a single side read request to peer, no NetCallback at peer will be triggered
 *
 * @param channel       [in] net channel
 * @param req           [in] request infomation, include 5 important varibles, local/remote address/key and size.
 * You can free it after called NetServiceOneSideDoneHandler.
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the interface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker thread.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in progress.
 * Return ERROR, cb will not be invoked.
 */
int Channel_Read(Net_Channel channel, Service_Request *req, Channel_Callback *cb);

/**
 * @brief Post multi single side read request to peer, no NetCallback at peer will be triggered
 *
 * @param channel       [in] net channel
 * @param req           [in] request infomation, fill with local different MRs, send to the same remote MR by local
 * MRs sequence, you can free it after called NetServiceOneSideDoneHandler. rKey/rAddress do not need to assign.
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the interface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker thread.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in progress.
 * Return ERROR, cb will not be invoked.
 */
int Channel_ReadSgl(Net_Channel channel, Service_SglRequest *req, Channel_Callback *cb);

/**
 * @brief Post a single side write request to peer, no NetCallback at peer will be triggered
 *
 * @param channel       [in] net channel
 * @param req           [in] request infomation, include 5 important varibles, local/remote address/key and size.
 * You can free it after called NetServiceOneSideDoneHandler.
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the interface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker thread.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in progress.
 * Return ERROR, cb will not be invoked.
 */
int Channel_Write(Net_Channel channel, Service_Request *req, Channel_Callback *cb);

/**
 * @brief Post multi single side write request to peer, no NetCallback at peer will be triggered
 *
 * @param channel       [in] net channel
 * @param req           [in] request infomation, fill with local different MRs, send to the same remote MR by local
 * MRs sequence, you can free it after called NetServiceOneSideDoneHandler. rKey/rAddress do not need to assign.
 * @param cb            [in] NetCallback for async send to peer. If cb is nullptr, the interface will block until
 * complete. And it means if set cb is nullptr in worker callback context, the interface will block worker thread.
 *
 * @return 0 if cb is null, it means the operation is cb successfully. Otherwise, it means the operation
 * is in progress.
 * Return ERROR, cb will not be invoked.
 */
int Channel_WriteSgl(Net_Channel channel, Service_SglRequest *req, Channel_Callback *cb);

/**
 * @brief close all connection in channel
 */
void Channel_Close(Net_Channel channel);

/**
 * @brief Set the timeout if the one side operation, include read/readSgl/write/writeSgl
 *
 * 1. timeout = 0: return immediately
 * 2. timeout < 0: never timeout, usually set to -1
 * 3. timeout > 0: Millisecond precision timeout.
 * Default timeout is -1.
 *
 * @param channel   [in] net channel.
 * @param timeout   [in] timeout in seconds.
 */
void Channel_SetOneSideTimeout(Net_Channel channel, int32_t timeout);

/**
 * @brief Set the timeout if the one side operation, include send/sendRaw/sendRawSgl/SyncCall/AsyncCall...etc
 *
 * 1. timeout = 0: return immediately
 * 2. timeout < 0: never timeout, usually set to -1
 * 3. timeout > 0: Millisecond precision timeout.
 * Default timeout is -1.
 *
 * @param channel   [in] net channel.
 * @param timeout   [in] timeout in seconds.
 */
void Channel_SetTwoSideTimeout(Net_Channel channel, int32_t timeout);

typedef enum {
    HIGH_LEVEL_BLOCK, /* spin-wait by busy loop */
    LOW_LEVEL_BLOCK,  /* full sleep */
} Channel_FlowCtrlLevel;

/**
 * @brief Flow control configuration item
 */
typedef struct {
    Channel_FlowCtrlLevel level; /* block level by different action */
    uint16_t intervalTimeMs;     /* interval time ms, range in [1, 1000] */
    uint64_t thresholdBytes;     /* threshold byte */
} Channel_FlowCtrlOptions;

/**
 * @brief config flow control
 */
int Channel_ConfigFlowControl(Net_Channel channel, Channel_FlowCtrlOptions options);

/**
 * @brief External log callback function
 *
 * @param level     [in] level, 0/1/2/3 represent debug/info/warn/error
 * @param msg       [in] message, log message with name:code-line-number
 */
typedef void (*Service_LogHandler)(int level, const char *msg);

/**
 * @brief Set external logger function
 *
 * @param h         [in] the log function ptr
 */
void Service_SetExternalLogger(Service_LogHandler h);

/**
 * @brief Woker polling type
 * 1 For RDMA:
 * C_SERVICE_BUSY_POLLING, means cpu 100% polling no matter there is request cb, better performance but cost dedicated CPU
 * C_SERVICE_EVENT_POLLING, waiting on OS kernal for request cb.
 * 2 For TCP/UDS:
 * only event polling is supported
 */
typedef enum {
    C_SERVICE_BUSY_POLLING = 0,
    C_SERVICE_EVENT_POLLING = 1,
} Service_WorkingMode;

/**
 * @brief OobType working mode
 */
typedef enum {
    C_SERVICE_OOB_TCP = 0,
    C_SERVICE_OOB_UDS = 1,
    C_SERVICE_OOB_UB = 2,
} Service_OobType;

/**
 * @brief worker load balance policy
 */
typedef enum {
    SERVICE_ROUND_ROBIN = 0,
    SERVICE_HASH_IP_PORT = 1,
} Service_LBPolicy;

/**
 * @brief CipherSuite mode
 */
typedef enum {
    C_SERVICE_AES_GCM_128 = 0,
    C_SERVICE_AES_GCM_256 = 1,
    C_SERVICE_AES_CCM_128 = 2,
    C_SERVICE_CHACHA20_POLY1305 = 3,
} Service_CipherSuite;

typedef enum {
    C_SERVICE_TLS_1_2 = 771,
    C_SERVICE_TLS_1_3 = 772,
} Service_TlsVersion;

typedef enum {
    C_SERVICE_OPENSSL = 0,
    C_SERVICE_HITLS = 1,
} Service_TlsMode;

/**
 * @brief Enum for secure type
 */
typedef enum {
    C_SERVICE_NET_SEC_DISABLED = 0,
    C_SERVICE_NET_SEC_ONE_WAY = 1,
    C_SERVICE_NET_SEC_TWO_WAY = 2,
} Service_SecType;

typedef enum {
    C_SERVICE_LOWLATENCY = 0,
    C_SERVICE_HIGHBANDWIDTH = 1,
} Service_UbcMode;

typedef struct {
    char netDeviceIpMask[NUM_256];      // ip mask for devices
    char netDeviceIpGroup[NUM_1024];    // ip group for devices
    uint16_t enableTls;                 // enable ssl
    Service_SecType secType;            // security type
    Service_TlsVersion tlsVersion;      // tls version, default is TLS1.3 (772)
    Service_CipherSuite cipherSuite;    // if tls enabled can set cipher suite, client and server should be same
    /* service setting */
    uint16_t enableRndv;        // enable rndv protocol
    uint16_t connectWindow;     // server side keep channel alive time in second
    uint32_t periodicThreadNum; // periodic check timeout req thread num, range [1, 4]
    /* worker setting */
    uint16_t dontStartWorkers;          // start worker or not
    Service_WorkingMode mode;           // worker polling modem could busy polling or event polling
    char workerGroups[NUM_64];          // worker groups, for example 1,3,3
    char workerGroupsCpuSet[NUM_128];   // worker groups cpu set, for example 1-1,2-5,na
    char workerGroupsThreadPriority[NUM_64];
    // worker thread priority [-20, 19], 19 is the lowest, -20 is the highest, 0 (default) means do not set priority
    int workerThreadPriority;
    /* connection setting */
    Service_OobType oobType;    // oob type,tcp or UDS, UDS cannot accept remote connection
    Service_LBPolicy lbPolicy;  // select worker load balance policy, default round-robin
    uint16_t maxTypeSize;       // service start() will check handlers with max size
    uint16_t magic;             // magic number fir c/s connect validation
    uint8_t version;            // program version used by connect validation
    /* heart beat setting */
    uint16_t heartBeatIdleTime;         // heart beat idle time, in seconds
    uint16_t heartBeatProbeTimes;        // heart beat probe times
    uint16_t heartBeatProbeInterval;    // heart beat probe interval, in secons
    /* options for tcp protocol only */
    // timeout during io (s), it should be [-1, 1024], -1 means do not set, 0 means never timeout during io
    int16_t tcpUserTimeout;
    bool tcpEnableNoDelay;      // tcp TCP_NODELAY option, false in default
    bool tcpSendZCopy;          // tcp whether copy request to inner memory, false in fault
    /**
     * The buffer sizes will be adjusted automically when these two variable are 0, and the performance would be
     * better
     */
    uint16_t tcpSendBufSize;    // tcp connection send buffer size in kernel, in KB
    uint16_t tcpReceiveBufSize; // tco connection send receive buf size in kernel, in KB
    /* options for rdma protocol only */
    uint32_t mrSendReceiveSegCount;     // memory region segment count for two side operation
    uint32_t mrSendReceiveSegSize;      // data size of memory region segment
    uint16_t completionQueueDepth;      // completion queue size of rdma
    uint16_t maxPostSendCountPerQP;     // max number request conld issue
    uint16_t prePostReceiveSizePerQP;   // pre post receive if qp
    uint16_t pollingBatchSize;          // polling batc size for worker
    uint16_t eventPollingTimeout;       // event polling timeout in us
    uint32_t qpSendQueueSize;           // max send working request of qp for rdma
    uint32_t qpReceiveQueueSize;        // max receive working request of qp for rdma
    uint32_t maxConnectionNum;          // max connection number
    uint8_t slave;
    char oobPortRange[NUM_16];          // port range when enable port auto selection
    uint16_t enableMultiRail;           // enable MultiRail
    Service_UbcMode ubcMode;
} Service_Options;

/**
 * @brief Options for mutltiple listeners
 */
typedef struct {
    char ip[NUM_16];            // ip to be listened
    uint16_t port;              // port to be listened
    uint16_t targetWorkerCount; // the count of workers can be dispatched to, for connections for this listener
} Service_OobListenOptions;

typedef struct {
    char name[NUM_96];          /* UDS name for listen or file path */
    uint16_t perm;              /* if 0 means not use file, otherwise use file an this perm as file perm */
    uint16_t targetWorkerCount; /* the count of target workers, if >= 1, the accepted socket will be attached to sub set to workers, 0 means all */
    uint16_t isCheck;           /* whether to verify the permission on the UDS file */
} Service_OobUDSListenerOptions;

typedef struct {
    uint16_t index;        /* connect type index for different op handlers */
    uint8_t epSize;        /* assign ep size in channel */
    Channel_CBType cbType; /* channel callback mode */
    uint8_t clientGrpNo;   /* indicates which server worker group to connect to */
    uint8_t serverGrpNo;   /* indicates which server worker group to connect to */
    uint32_t flags;        /* for server default 0, for client sync ep : SER_C_CHANNEL_SELF_POLLING, SER_C_CHANNEL_EVENT_POLLING */
    unsigned long memid;
} Service_ConnectOpt;

typedef enum {
    C_CHANNEL_BROKEN_ALL = 0,   /* when one ep broken, all eps broken */
    C_CHANNEL_RECONNECT = 1,    /* when one ep broken, try re-connect first. If re-connect fail, broken all eps */
    C_CHANNEL_KEEP_ALIVE = 2,   /* when one ep broken, keep left eps alive until all eps broken */
} Service_ChannelPolicy;

/**
 * @brief Callback function definition
 * 1) new endpoint connected from client, only need to register this at server side
 * 2) endpoint is broken, called when RDMA qp detection error or broken
 */
typedef int (*Service_ChannelHandler)(Net_Channel channel, uint64_t usrCtx, const char *payLoad);

/**
 * @brief Callback function definition
 *
 * it is called when the following cases happen
 * 1) post send cb
 * 2) read cb
 * 3) write cb
 *
 * @note
 * 1) ctx is a thread local static variable.
 * 2) msgData need to copy to another space properly
 * 3) ep can be transferred to another thread for further reply or other stuff
 * in this case, need to call Channel_Refer() to increase reference count
 * and call Channel_Destroy() after to decrease the reference count
 */
typedef int (*Service_RequestHandler)(Service_Context ctx, uint64_t usrCtx);

/**
 * @brief Idle callback function, when worker thread idle, this function will be called
 *
 * @param wkrGrpIdx         [in] worker group index in on net driver
 * @param idxInGrp          [in] worker index in the group
 * @param usrCtx            [in] user context
 */
typedef void (*Service_IdleHandler)(uint8_t wkrGrpIdx, uint16_t idxInGrp, uint64_t usrCtx);

/**
 * @brief rndv protocol memory allocate callback
 */
typedef int (*Service_RndvMemAllocate)(uint64_t size, uintptr_t *address, uint32_t *key);

/**
 * @brief rndv protocal memory free callback
 */
typedef int (*Service_RndvMemFree)(uintptr_t address);

/**
 * @brief rndv protocol operate handler
 */
typedef int (*Service_RndvHandler)(Service_RndvContext context);

/**
 * @brief Enum for callback register [new endpoint connected or endpoint broken]
 */
typedef enum {
    C_CHANNEL_NEW = 0,
    C_CHANNEL_BROKEN = 1,
} Service_HandlerType;

/**
 * @brief Enum for callback register [request received, request posted, read/write cb]
 */
typedef enum {
    C_SERVICE_REQUEST_RECEIVED = 0,
    C_SERVICE_REQUEST_POSTED = 1,
    C_SERVICE_READWRITE_DONE = 2,
} Service_OpHandlerType;

typedef enum {
    C_SERVICE_RDMA = 0,
    C_SERVICE_TCP = 1,
    C_SERVICE_UDS = 2,
    C_SERVICE_SHM = 3,
    C_SERVICE_UB = 4,
    C_SERVICE_UBOE = 5,
    C_SERVICE_UBC = 6,
    C_SERVICE_HSHMEM = 7,
} Service_Type;

/**
 * @brief Create a service
 *
 * @param t                 [in] type of service
 * @param name              [in] the name of service
 * @param startOobSvr       [in] 0 or 1, 1 to start Oob server, 0 don't start Oob server
 * @param service           [out] created service address
 *
 * @return 0, if created successfully
 */
int Service_Create(Service_Type t, const char *name, uint8_t startOobSvr, Net_Service *service);

/**
 * @brief Set out of bound ip and port
 *
 * @param service           [in] created service address
 * @param ip                [in] ip address
 * @param port              [out] port
 */
void Service_SetOobIpAndPort(Net_Service service, const char *ip, uint16_t port);

/**
 * @brief Set out of bound ip adn port
 *
 * @param service           [in] the address of driver
 * @param ipArray           [out] oob ip list
 * @param port              [out] oob port list
 * @param length            [out] the length of ipArray and portArray
 */
bool Service_GetOobIpAndPort(Net_Service service, char ***ipArray, uint16_t **portArray, int *length);

/**
 * @brief Start the driver, start oob accept thread (server only) and RDMA polling thread
 *
 * @param service           [in] the address of service
 * @param options           [in] the options of the service
 *
 * @return 0 if successful
 */
int Service_Start(Net_Service service, Service_Options options);

/**
 * @brief Stop the service
 *
 * @param service           [in] the address of service
 */
void Service_Stop(Net_Service service);

/**
 * @brief Destroy the service
 *
 * @param service           [in] the address of service
 *
 * @return 0 if destroy successful
 */
int Service_Destroy(Net_Service service);

/**
 * @brief Connect to server
 *
 * @param service           [in] the address of service
 * @param oobIpOrName       [in] oob ip or name to connect, set ip for tcp and name for uds
 * @param oobPort           [in] only need to set when tcp oob
 * @param payload           [in] payload transfered to peer, will be got EP Connected Callback at server
 * @param channel           [out] connected channel
 * @param options           [in] variable argument for specific use
 *
 * @return 0 successful
 */
int Service_Connect(Net_Service service, const char *oobIpOrName, uint16_t oobPort, const char *payload,
                    Net_Channel *channel, Service_ConnectOpt *options);

/**
 * @brief Reconnect to server, recover channel
 *
 * @param service           [in] the address if service
 * @param channel           [in] connected channel
 *
 * @return 0 successful
 */
int Service_Reconnect(Net_Service service, Net_Channel channel);

/**
 * @brief Increase the internal reference count, need to call this when forwading the context to another thread to
 * process
 *
 * @param channel           [in] the address of channel
 */
void Channel_Refer(Net_Channel channel);

/**
 * @brief DeRefer the internal reference count
 *
 * @param channel           [in] the address of channel
 */
void Channel_DeRefer(Net_Channel channel);

/**
 * @brief Destroy the channel
 *
 * @param channel           [in] the address of channel
 */
void Channel_Destroy(Net_Channel channel);

/**
 * @brief Register a memory region, the memory will allocated internally
 *
 * @param service           [in] the address of service
 * @param size              [in] size of the meory region
 * @param mr                [out] memory region registered
 *
 * @return 0 successful
 */
int Service_RegisterMemoryRegion(Net_Service service, uint64_t size, Service_MemoryRegion *mr);

/**
 * @brief Register a memory region, the memory need to passed in
 *
 * @param service   [in] the address of server
 * @param address   [in] memory pointer that needs to be registered
 * @param size      [in] size of memory region
 * @param mr        [out] memory region regitstered to network card
 *
 * @return 0 for sucess and others for failure.
 */
int Service_RegisterAssignMemoryRegion(Net_Service service, uintptr_t address, uint64_t size, Service_MemoryRegion *mr);

/**
 * @brief Parse the memory region, get info
 *
 * @param mr        [in] memory region registered
 * @param info      [in] memory region info
 *
 * @return 0 for sucess and others for failure.
 */
int Service_GetMemoryRegionInfo(Service_MemoryRegion mr, Service_MemoryRegionInfo *info);

/**
 * @brief Unregister the memory region
 *
 * @param service   [in] the address of server
 * @param mr        [in] memory region registered
 */
void Service_DestroyMemoryRegion(Net_Service service, Service_MemoryRegion mr);

/**
 * @brief Register callback function for channel
 *
 * @param service       [in] the address of service
 * @param t             [in] handle type, could be C_CHANNEL_NET or C_CHANNEL_BROKEN
 * @param h             [in] callback function address
 *
 * @return an inner handler address, for un-register in case of memory leak. Return 0 means failed
 */
uintptr_t Service_RegisterChannelHandler(Net_Service service, Service_HandlerType t, Service_ChannelHandler h,
                                         Service_ChannelPolicy policy, uint64_t usrCtx);

/**
 * @brief Register callback function for channel operation
 *
 * @param service       [in] the address of service
 * @param i             [in] channel type index, range [0, 15]
 * @param t             [in] handle type, could be C_SERVICE_REQUEST_RECEIVED or C_SERVICE_REQUEST_POSTED or
 * C_SERVICE_READWRITE_DONE
 * @param h             [in] callback function address
 *
 * @return an inner handler address, for un-register in case of memory leak. Return 0 means failed
 */
uintptr_t Service_RegisterOpHandler(Net_Service service, uint16_t i, Service_OpHandlerType t, Service_RequestHandler h,
                                    uint64_t usrCtx);

/**
 * @brief Register callback function for worker idle
 *
 * @param service       [in] the address of service
 * @param h             [in] handler
 * @param usrCtx        [in] user context, passed to callback function
 *
 * @return an inner handler address, for un-register in case of memory leak. Return 0 means failed
 */
uintptr_t Service_RegisterIdleHandler(Net_Service service, Service_IdleHandler h, uint64_t usrCtx);

/**
 * @brief Register callback function for rndv memory allocate
 *
 * @param service       [in] the address of service
 * @param t             [in] handler
 *
 * @return an inner handler address, for un-register in case of memory leak. Return 0 means failed
 */
uintptr_t Service_RegisterAllocateHandler(Net_Service service, Service_RndvMemAllocate h);

/**
 * @brief Register callback function for rndv memory free
 *
 * @param service       [in] the address of service
 * @param t             [in] handler
 *
 * @return an inner handler address, for un-register in case of memory leak. Return 0 means failed
 */
uintptr_t Service_RegisterFreeHandler(Net_Service service, Service_RndvMemFree h);

/**
 * @brief Register callback function for rndv op handler
 *
 * @param service       [in] the address of service
 * @param t             [in] handler
 *
 * @return an inner handler address, for un-register in case of memory leak. Return 0 means failed
 */
uintptr_t Service_RegisterRndvOpHandler(Net_Service service, Service_RndvHandler h);

/**
 * @brief Enum for tls callback, set peer cert verify type
 */
typedef enum {
    C_VERIFY_BY_NONE = 0,
    C_VERIFY_BY_DEFAULT = 1,
    C_VERIFY_BY_CUSTOM_FUNC = 2,
} Net_PeerCertVerifyType;

/**
 * @brief keyPass       [in] erase function
 * @param keyPass       [in] the memory address of keyPass
 */
typedef void (*Net_TlsKeyPassErase)(char *keyPass, int len);

/**
 * @brief The cert verify function
 *
 * @param x509          [in] the x509 object of CA
 * @param crlPath       [in] the crl file path
 *
 * @return -1 for failed, and 1 for success
 */
typedef int (*Net_TlsCertVerify)(void *x509, const char *crlPath);

/**
 * @brief Get the certificate file of public key
 *
 * @param name          [out] name
 * @param certPath      [out] the path of certificate
 */
typedef int (*Net_TlsGetCertCb)(const char *name, char **certPath);

/**
 * @brief Get private key file's path and length, and get the keyPass
 * @param name          [out] the name
 * @param priKeyPath    [out] the path of private key
 * @param keyPass       [out] the keyPass
 * @param erase         [out] the erase function
 */
typedef int (*Net_TlsGetPrivateKeyCb)(const char *name, char **priKeyPath, char **keyPass, Net_TlsKeyPassErase *erase);

/**
 * @brief Get the CA and verify
 * @param name          [out] the name
 * @param caPath        [out] the path of CA file
 * @param crlPath       [out] the crl file path
 * @param verifyType    [out] the type of verify in [VERIFY_BY_NONE, VERIFY_BY_DEFAULT, VERIFY_BY_CUSTOM_FUNC]
 * @param verify        [out] the verify function, only effect in VERIFY_BY_CUSTOM_FUNC mode
 */
typedef int (*Net_TlsGetCACb)(const char *name, char **caPath, char **crlPath, Net_PeerCertVerifyType *verifyType,
                              Net_TlsCertVerify *verify);

/**
 * @brief Register callback function for tls enable
 *
 * @param service       [in] the address if driver
 * @param certCb        [in] callback to get cert
 * @param priKeyCb      [in] callback to get private key
 * @param caCb          [in] callback to get ca
 *
 * @return an inner handler address, for un-register in case of memory leak
 */
uintptr_t Service_RegisterTLSCb(Net_Service service, Net_TlsGetCertCb certCb, Net_TlsGetPrivateKeyCb priKeyCb,
                                Net_TlsGetCACb caCb);

/**
 * @brief External log callback function
 *
 * @param level         [in] level, 0/1/2/3 represent debug/info/warn/error
 * @param msg           [in] message, log message wit name:code-line-number
 */
typedef void (*Net_LogHandler)(int level, const char *msg);

/**
 * @brief Alllocatir cache tier policy
 */
typedef enum {
    C_TIER_TIMES = 0, /* tier by times of min-block-size */
    C_TIER_POWER = 1, /* tier by power of min-block-size */
} Net_MemoryAllocatorCacheTierPolicy;

/**
 * @brief Type of allocator
 */
typedef enum {
    C_DYNAMIC_SIZE = 0,            /* allocate dynamic memory size, there is alignment with X KB */
    C_DYNAMIC_SIZE_WITH_CACHE = 1, /* allocator with dynamic memory size, with pre-allocate cache for performance */
} Net_MemoryAllocatorType;

/**
 * @brief Options for memory allocator
 */
typedef struct {
    uintptr_t address;                                    /* base address if large range of memory for allocator */
    uint64_t size;                                        /* size of large memory chunk */
    uint32_t minBlockSize;                                /* min size of block, more than 4 KB is required */
    uint32_t bucketCount;                                 /* default size of hash bucket */
    uint16_t alignedAddress;                              /* force to align the memory block allocated, 0 means not align, 1 means align */
    uint16_t cacheTierCount;                              /* for MEM_DYNAMIC_SIZE_WITH_CACHE only */
    uint16_t cacheBlockCountPerTier;                      /* for MEM_DYNAMIC_SIZE_WITH_CACHE only */
    Net_MemoryAllocatorCacheTierPolicy cacheTierPolicy; /* tier policy */
} Net_MemoryAllocatorOptions;

/**
 * @brief memory allocator pointer
 */
typedef uintptr_t Net_MemoryAllocator;

/**
 * @brief Create a memory allocator
 *
 * @param t             [in] type of alloctor
 * @param options       [in] options for creation
 * @param allocator     [out] memory allocator created
 *
 * @return 0 for sucess and others for failure.
 */
int Net_MemoryAllocatorCreate(
    Net_MemoryAllocatorType t, Net_MemoryAllocatorOptions *options, Net_MemoryAllocator *allocator);

/**
 * @brief Destroy a memory allocator
 *
 * @param allocator     [in] memory allocator created by Net_MemoryAllocatorCreate
 *
 * @return 0 for sucess and others for failure.
 */
int Net_MemoryAllocatorDestroy(Net_MemoryAllocator allocator);

/**
 * @brief Set the memory region key
 *
 * @param allocator     [in] memory allocator created by Net_MemoryAllocatorCreate
 * @param mrKey         [in] key to access memory region
 *
 * @return 0 for sucess and others for failure.
 */
int Net_MemoryAllocatorSetMrKey(Net_MemoryAllocator allocator, uint32_t mrKey);

/**
 * @brief Get the address's memory offset in allocator based in base address
 *
 * @param allocator     [in] memory allocator created by Net_MemoryAllocatorCreate
 * @param address       [in] user's current address
 * @param offset        [out] offset comparing to base address
 *
 * @return 0 for sucess and others for failure.
 */
int Net_MemoryAllocatorMemOffset(Net_MemoryAllocator allocator, uintptr_t address, uintptr_t *offset);

/**
 * @brief Get free memory size in alloctor memory
 *
 * @param allocator     [in] memory allocator created by Net_MemoryAllocatorCreate
 * @param size          [out] free memory size
 *
 * @return 0 for sucess and others for failure.
 */
int Net_MemoryAllocatorFreeSize(Net_MemoryAllocator allocator, uintptr_t *size);

/**
 * @brief Allocate memory area
 *
 * @param alloactor     [in] memory allocator created by Net_MemoryAllocatorCreate
 * @param size          [in] memory size user needs
 * @param address       [out] allocaoted memory address
 * @param key           [out] memory region key set by Net_MemoryAllocatorSetMrKey
 *
 * @return 0 for sucess and others for failure.
 */
int Net_MemoryAllocatorAllocate(Net_MemoryAllocator allocator, uint64_t size, uintptr_t *address, uint32_t *key);

/**
 * @brief Free memory area
 *
 * @param alloactor     [in] memory allocator created by Net_MemoryAllocatorCreate
 * @param address       [in] memory address to free, allocated by Net_MemoryAllocatorAllocate
 *
 * @return 0 for sucess and others for failure.
 */
int Net_MemoryAllocatorFree(Net_MemoryAllocator allocator, uintptr_t address);

/**
 * @brief Set external logger function
 *
 * @param h             [in] the log function ptr
 */
void Net_SetExternalLogger(Net_LogHandler h);

#ifdef __cplusplus
}
#endif

#endif