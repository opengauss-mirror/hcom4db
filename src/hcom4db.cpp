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
 * hcom4db.cpp
 *
 *
 * IDENTIFICATION
 *    src/hcom4db.cpp
 *
 * -------------------------------------------------------------------------
 */
#include <map>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <string>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <climits>
#include "securec.h"
#include "hcom4db_log.h"
#include "hcom4db_define.h"
#include "hcom4db_utils.h"
#include "hcom4db_hcom.h"
#include "hcom4db_dlopen_hcom.h"
#include "hcom4db.h"
using namespace hcom4dbDefine;
namespace {
constexpr uint16_t OCK_MAX_PORT_NUM = 65535;
constexpr uint16_t OCK_MIN_PORT_NUM = 1024;
constexpr uint32_t OCK_MAX_MESSAG_SIZE = 64 * 1024;
constexpr uint32_t OCK_MAX_MESSAG_COUNT = 1024;
constexpr uint32_t OCK_MAX_SEND_QUEUE_SIZE = 2048;
constexpr uint32_t OCK_MAX_RECEIVE_QUEUE_SIZE = 2048;
constexpr uint32_t OCK_MAX_PRE_SEND_RECEIVE_SIZE = 1024;
constexpr uint32_t OCK_MAX_COMPLETE_QUEUE_SIZE = 8192;
constexpr uint32_t OCK_MAX_PORT_RANGE_STR_LEN = 16;
constexpr uint64_t MR_ADDRESS_SIZE = static_cast<unsigned long long>(OCK_MAX_MESSAG_SIZE * OCK_MAX_MESSAG_COUNT);
constexpr uint32_t MR_MAX_SIZE = OCK_MAX_MESSAG_SIZE;
constexpr uint32_t MR_MIN_SIZE = 4 * 1024;
constexpr uint32_t MR_PRE_ALLOC_CNT = 10;
constexpr uint32_t MR_BUCKET_MAP_SIZE = 1024;
constexpr uint32_t IP_MAX_LEN = 64;
constexpr uint32_t OCK_MAX_IOV_COUNT = 4;
constexpr uint16_t DEFAULT_CONNECTION_EP_SIZE = 1;
constexpr uint16_t DEFAULT_HEARTBEAT_IDLE_TIME = 1;
constexpr uint16_t MAX_MESSAGE_ID_SIZE = 1000;
constexpr uint32_t MAX_SEGMENT_SIZE = 943718400;
constexpr uint32_t MAX_SEGMENT_COUNT = 65535;
constexpr uint16_t MAX_CALL_WITH_PARAMS_COUNT = 8;
constexpr uint16_t MAX_MESSAGE_SGL_COUNT = 4;
constexpr const char *HCOM4DB_SO_NAME = "libhcom.so";
constexpr const char *HCOM4DB_ENV_PATH = "HCOM4DB_LIB_PATH";
constexpr int32_t DEFAULT_TIMEOUT_TIME = 1000;
constexpr int32_t MILLOSECOND_PER_SECOND = 1000;
}  // namespace

static OckRpcLogHandler g_logFunc = nullptr;
static TlsFuncs g_tlsFunc = TlsFuncs();
static Service_Options g_options = Service_Options();
static Service_Type g_connectionType = C_SERVICE_RDMA;
static std::map<int16_t, OckRpcMsgHandler> g_handleMap;
static std::map<int16_t, OckRpcMsgRndvHandler> g_rndvHandleMap;
static std::map<std::string, Net_Service> g_serviceMap;
static std::shared_mutex serviceMutex;
static std::shared_mutex serviceAddMutex;
static std::shared_mutex serviceAddRndvMutex;
static std::map<OckRpcClient, Net_MemoryAllocator> g_memoryAllocator;
static std::map<OckRpcClient, OckRpcMrInfo> g_memoryRegion;
static int32_t g_timeoutTime = DEFAULT_TIMEOUT_TIME;

static int Hcom4dbGetLibPath(char* rpcPath, uint32_t rpcPathLen)
{
    char* tmp = getenv(HCOM4DB_ENV_PATH);
    if (tmp == nullptr) {
        OCK_RPC_LOG_ERROR("Hcom4db getenv " << HCOM4DB_ENV_PATH << " failed.");
        return OCK_RPC_ERR;
    }

    if (rpcPathLen < strlen(tmp)) {
        OCK_RPC_LOG_ERROR("ENV len is more than path arr size:" << rpcPathLen);
        return OCK_RPC_ERR;
    }

    if (realpath(tmp, rpcPath) == nullptr) {
        OCK_RPC_LOG_ERROR("realpath HCOM4DB_ENV_PATH failed");
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static int Hcom4dbInitDlopenSo(void)
{
    char rpcPath[PATH_MAX] = {0};
    int ret = Hcom4dbGetLibPath(rpcPath, sizeof(rpcPath));
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    // init ockrpc dlopen
    char ockRpcDlPath[PATH_MAX] = {0};
    ret = snprintf_s(ockRpcDlPath, PATH_MAX, PATH_MAX - 1, "%s/%s", rpcPath, HCOM4DB_SO_NAME);
    if (ret < 0) {
        OCK_RPC_LOG_ERROR("construct hcom dl failed, ret " << ret);
        return OCK_RPC_ERR;
    }
    ret = InitOckRpcDl(ockRpcDlPath, PATH_MAX);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Hcom4db init hcomDl failed, ret " << ret);
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

OckRpcServerContext OckRpcServerCtxBuilderThreadLocal(OckRpcServer server)
{
    return 0;
}

void OckRpcServerCtxCleanupThreadLocal(OckRpcServer server, OckRpcServerContext ctx)
{
    return;
}

static int OckRpcRegisterMr(OckRpcServer server, uintptr_t address, size_t memSize, Service_MemoryRegionInfo* info,
                            Service_MemoryRegion* mr)
{
    int ret = Service_RegisterAssignMemoryRegion(server, address, memSize, mr);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Register mr failed, ret(" << ret << ").");
        return OCK_RPC_ERR;
    }

    ret = Service_GetMemoryRegionInfo(*mr, info);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Get mr info failed, ret(" << ret << ").");
        return OCK_RPC_ERR;
    }
    return OCK_RPC_OK;
}

static int OckRpcString2Num(std::map<std::string, const char *> &configMap, const char *key, uint32_t &size)
{
    auto iter = configMap.find(key);
    if (iter != configMap.end()) {
        try {
            size = std::stoi(iter->second);
        } catch (const std::invalid_argument &e) {
            OCK_RPC_LOG_ERROR("Invalid argument " << key << " :" << e.what());
            return OCK_RPC_ERR;
        } catch (const std::out_of_range &e) {
            OCK_RPC_LOG_ERROR("Out of range " << key << " :" << e.what());
            return OCK_RPC_ERR;
        }
    }

    return OCK_RPC_OK;
}

static int OckRpcSetAllocatorOption(std::map<std::string, const char *> &configMap, Net_MemoryAllocatorOptions *option)
{
    uint32_t maxSize = OCK_MAX_MESSAG_SIZE;
    int ret = OckRpcString2Num(configMap, OCK_RPC_STR_PRE_SEGMENT_SIZE, maxSize);
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }
    if (maxSize > MAX_SEGMENT_SIZE) {
        OCK_RPC_LOG_ERROR("Invalid params, OCK_RPC_STR_PRE_SEGMENT_SIZE should be less than " << MAX_SEGMENT_SIZE);
        return OCK_RPC_ERR;
    }

    uint32_t maxCount = OCK_MAX_MESSAG_COUNT;
    ret = OckRpcString2Num(configMap, OCK_RPC_STR_PRE_SEGMENT_COUNT, maxCount);
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }
    if (maxCount > MAX_SEGMENT_COUNT) {
        OCK_RPC_LOG_ERROR("Invalid params, OCK_RPC_STR_PRE_SEGMENT_COUNT should be less than " << MAX_SEGMENT_COUNT);
        return OCK_RPC_ERR;
    }

    uint64_t allocatorSize = static_cast<uint64_t>(maxSize * maxCount);
    if (allocatorSize == 0) {
        OCK_RPC_LOG_WARN("Memory size for CallWithParams is 0, check it.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }
    if (allocatorSize < MR_MIN_SIZE) {
        allocatorSize = MR_MIN_SIZE;
        OCK_RPC_LOG_WARN("Memory size < 4096, has setted it to min size 4096");
    }

    uintptr_t beginAddr = reinterpret_cast<uintptr_t>(std::malloc(allocatorSize));
    if (beginAddr == 0) {
        OCK_RPC_LOG_ERROR("Memory allocator malloc failed, size:" << allocatorSize);
        return OCK_RPC_ERR;
    }

    option->address = beginAddr;
    option->size = allocatorSize;
    option->minBlockSize = MR_MIN_SIZE;
    option->alignedAddress = true;
    uint32_t multiple = maxSize / MR_MIN_SIZE;
    option->cacheTierCount = Calculate2PowerTimes(multiple);
    option->cacheBlockCountPerTier = MR_PRE_ALLOC_CNT;
    option->bucketCount = MR_BUCKET_MAP_SIZE;
    option->cacheTierPolicy = C_TIER_POWER;

    return OCK_RPC_OK;
}

static int OckRpcPrepareMemAllocator(
    OckRpcServer server, OckRpcClient client, std::map<std::string, const char *> &configMap)
{
    Net_MemoryAllocatorOptions option;
    int ret = OckRpcSetAllocatorOption(configMap, &option);
    if (ret == OCK_RPC_ERR_INVALID_PARAM) {
        return OCK_RPC_OK;
    }
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    Net_MemoryAllocator allocator = 0;
    ret = Net_MemoryAllocatorCreate(C_DYNAMIC_SIZE_WITH_CACHE, &option, &allocator);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Failed to create memory allocator.");
        return OCK_RPC_ERR;
    }

    Service_MemoryRegionInfo info;
    Service_MemoryRegion mr;
    ret = OckRpcRegisterMr(server, option.address, option.size, &info, &mr);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Failed to register memory region.");
        return OCK_RPC_ERR;
    }

    ret = Net_MemoryAllocatorSetMrKey(allocator, info.lKey);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Failed to set key for allocator.");
        return OCK_RPC_ERR;
    }

    auto iter = g_memoryAllocator.find(client);
    if (iter == g_memoryAllocator.end()) {
        g_memoryAllocator.insert(std::make_pair(client, allocator));
    }

    OckRpcMrInfo mrInfo = OckRpcMrInfo();
    mrInfo.mr = mr;
    mrInfo.mrInfo = info;
    mrInfo.server = server;
    auto iterMr = g_memoryRegion.find(client);
    if (iterMr == g_memoryRegion.end()) {
        g_memoryRegion.insert(std::make_pair(client, mrInfo));
    }

    return OCK_RPC_OK;
}

static int OckRpcMemAlloc(OckRpcClient client, uint64_t memSize, uintptr_t *addr, uint32_t *key)
{
    int ret = Net_MemoryAllocatorAllocate(g_memoryAllocator[client], memSize, addr, key);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Hcom memory allocate failed, ret(" << ret << ").");
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static void OckRpcMemFree(OckRpcClient client, uintptr_t addr)
{
    int32_t ret = Net_MemoryAllocatorFree(g_memoryAllocator[client], addr);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Hcom memory free failed, ret(" << ret << ").");
    }

    return;
}

static int OckRpcTlsGetCertCb(const char *name, char **certPath)
{
    g_tlsFunc.getCert(const_cast<const char **>(certPath));
    if (*certPath == nullptr) {
        OCK_RPC_LOG_ERROR("Cert path is nullptr, check your set.");
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static void OckRpcTlsKeypassEraseCb(char *keyPass, int len)
{
    g_tlsFunc.eraseKeyPass(keyPass);
}

static int OckRpcTlsGetPrivateKeyCb(const char *name, char **priKeyPath, char **keyPass, Net_TlsKeyPassErase *erase)
{
    g_tlsFunc.getPriKey(const_cast<const char **>(priKeyPath), keyPass, &g_tlsFunc.eraseKeyPass);
    *erase = OckRpcTlsKeypassEraseCb;
    if (*priKeyPath == nullptr || *keyPass == nullptr || *erase == nullptr) {
        OCK_RPC_LOG_ERROR("Private key is nullptr, check your set.");
        return OCK_RPC_ERR;
    }
    return OCK_RPC_OK;
}

static int OckRpcTlsGetCACb(
    const char *name, char **caPath, char **crlPath, Net_PeerCertVerifyType *verifyType, Net_TlsCertVerify *verify)
{
    g_tlsFunc.getCaAndVerify(const_cast<const char **>(caPath), const_cast<const char **>(crlPath), verify);
    *verifyType = C_VERIFY_BY_CUSTOM_FUNC;
    if (*caPath == nullptr || *crlPath == nullptr || verify == nullptr) {
        OCK_RPC_LOG_ERROR("Ca path or crl path is nullptr, check your set.");
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static int32_t OckRpcNewChannel(Net_Channel newCh, uint64_t usrCtx, const char *payLoad)
{
    OCK_RPC_LOG_INFO("channel new.");
    return OCK_RPC_OK;
}

static int32_t OckRpcBrokenChannel(Net_Channel newCh, uint64_t usrCtx, const char *payLoad)
{
    Channel_DeRefer(newCh);
    OCK_RPC_LOG_INFO("channel broken.");
    return OCK_RPC_OK;
}

static int OckRpcReceiveHandle(Service_Context ctx, uint64_t usrCtx)
{
    int ret;
    OckRpcMessage msg;
    msg.data = Service_GetMessageData(ctx);
    if (msg.data == nullptr) {
        OCK_RPC_LOG_ERROR("Receiver get nullptr message.");
        return OCK_RPC_ERR;
    }
    msg.len = Service_GetMessageDataLen(ctx);
    Service_ContextType ctxType;
    ret = Service_GetContextType(ctx, &ctxType);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Get context type failed.");
        return OCK_RPC_ERR;
    }

    uint16_t msgId = 0;
    if (ctxType == SERVICE_RECEIVED_RAW) {
        RawMessageHeader *head = (RawMessageHeader *)msg.data;
        msgId = head->msgId;
        msg.data = (uint8_t *)msg.data + sizeof(RawMessageHeader);
        msg.len -= sizeof(RawMessageHeader);
    } else {
        Service_OpInfo opInfo;
        ret = Service_GetOpInfo(ctx, &opInfo);
        if (ret != OCK_RPC_OK) {
            OCK_RPC_LOG_ERROR("Receiver get opInfo failed, ret(" << ret << ").");
            return OCK_RPC_ERR;
        }
        msgId = opInfo.opCode;
    }

    serviceAddMutex.lock_shared();
    auto iter = g_handleMap.find(msgId);
    if (iter != g_handleMap.end()) {
        iter->second(ctx, msg);
    } else {
        OCK_RPC_LOG_INFO("No match msg handle id:" << msgId << ", do nothing.");
    }
    serviceAddMutex.unlock();
    return OCK_RPC_OK;
}

static int OckRpcPostHandle(Service_Context ctx, uint64_t usrCtx)
{
    // be called when post
    // for asyan, you can pass cb .etc here by usrCtx
    return 0;
}

static int OckRpcOneSideDone(Service_Context ctx, uint64_t usrCtx)
{
    // be called when write/read
    // for asyan, you can pass cb .etc here by usrCtx
    return 0;
}

static int OckRpcInitConfigMap(OckRpcCreateConfig *configs, std::map<std::string, const char *> &configMap)
{
    if (configs->mask & OCK_RPC_CONFIG_USE_RPC_CONFIGS) {
        if (configs->configs.pairs == nullptr) {
            OCK_RPC_LOG_ERROR("Invalid configs, pairs is nullptr");
            return OCK_RPC_ERR;
        }
        if (configs->configs.size < 0) {
            OCK_RPC_LOG_ERROR("Invalid configs size.");
            return OCK_RPC_ERR;
        }
        for (int i = 0; i < configs->configs.size; i++) {
            if (configs->configs.pairs[i].key == nullptr || configs->configs.pairs[i].value == nullptr) {
                OCK_RPC_LOG_ERROR("Invalid configs, key or value is nullptr, check");
                return OCK_RPC_ERR;
            }
            configMap.insert(std::make_pair(configs->configs.pairs[i].key, configs->configs.pairs[i].value));
        }
    }

    return OCK_RPC_OK;
}


static int OckRpcCreateServer(std::map<std::string, const char *> &configMap, OckRpcServer *server)
{
    const char* serviceName = "service";
    auto iter = configMap.find(OCK_RPC_STR_SERVER_NAME);
    if (iter != configMap.end()) {
        serviceName = iter->second;
    }
    if (strlen(serviceName) > NUM_64) {
        OCK_RPC_LOG_ERROR("Invalid serviceName, bigger than 64");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    Service_Type type = C_SERVICE_RDMA;
    auto iterType = configMap.find(OCK_RPC_STR_COMMUNICATION_TYPE);
    const char *expectType = "TCP";
    const char *expectTypeRdma = "RDMA";
    if (iterType != configMap.end()) {
        if (!strcmp(iterType->second, expectType)) {
            type = C_SERVICE_TCP;
            g_connectionType = C_SERVICE_TCP;
            OCK_RPC_LOG_INFO("Create transport type TCP");
        } else if (!strcmp(iterType->second, expectTypeRdma)) {
            type = C_SERVICE_RDMA;
            g_connectionType = C_SERVICE_RDMA;
            OCK_RPC_LOG_INFO("Create transport type RDMA");
        } else {
            OCK_RPC_LOG_ERROR("Invalid transport type, should be TCP or RDMA");
            return OCK_RPC_ERR_INVALID_PARAM;
        }
    }

    int ret = Service_Create(type, serviceName, true, server);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Create server failed, ret(" << ret << ").");
        return OCK_RPC_ERR;
    }

    serviceMutex.lock_shared();
    auto iterName = g_serviceMap.find(serviceName);
    if (iterName == g_serviceMap.end()) {
        g_serviceMap.insert(std::make_pair(serviceName, *server));
    }
    serviceMutex.unlock_shared();

    return OCK_RPC_OK;
}

static void OckRpcRegisterChannelHandle(Net_Service service, std::map<std::string, const char *> &configMap)
{
    OckRpcClientHandler newFunc = OckRpcNewChannel;
    auto newIter = configMap.find(OCK_RPC_STR_CLIENT_NEW);
    if (newIter != configMap.end()) {
        uintptr_t newPtr = (uintptr_t)const_cast<char *>(newIter->second);
        newFunc = reinterpret_cast<OckRpcClientHandler>(newPtr);
    }

    OckRpcClientHandler breakFunc = OckRpcBrokenChannel;
    auto breakIter = configMap.find(OCK_RPC_STR_CLIENT_BREAK);
    if (breakIter != configMap.end()) {
        uintptr_t breakPtr = (uintptr_t)const_cast<char *>(breakIter->second);
        breakFunc = reinterpret_cast<OckRpcClientHandler>(breakPtr);
    }

    Service_RegisterChannelHandler(service, C_CHANNEL_NEW, newFunc, C_CHANNEL_BROKEN_ALL, 1);
    Service_RegisterChannelHandler(service, C_CHANNEL_BROKEN, breakFunc, C_CHANNEL_BROKEN_ALL, 1);
}

static int OckRpcSetWorkThreadConfigs(std::map<std::string, const char *> &configMap, Service_Options *options)
{
    int ret;
    options->mode = C_SERVICE_EVENT_POLLING;
    const char *expectType = "yes";
    const char *expectTypeNo = "no";
    auto iter = configMap.find(OCK_RPC_STR_ENABLE_POLL_WORKER);
    if (iter != configMap.end()) {
        if (!strcmp(iter->second, expectType)) {
            OCK_RPC_LOG_INFO("Set busy polling");
            options->mode = C_SERVICE_BUSY_POLLING;
        } else if (!strcmp(iter->second, expectTypeNo)) {
            OCK_RPC_LOG_INFO("Set event polling");
            options->mode = C_SERVICE_EVENT_POLLING;
        } else {
            OCK_RPC_LOG_ERROR("Invalid param, CK_RPC_STR_ENABLE_POLL_WORKER should be yes or no");
            return OCK_RPC_ERR;
        }
    } else {
        OCK_RPC_LOG_INFO("Set event polling");
    }

    iter = configMap.find(OCK_RPC_STR_WRK_THR_GRP);
    if (iter != configMap.end()) {
        ret = strcpy_s(options->workerGroups, NUM_64, iter->second);
        if (ret != OCK_RPC_OK) {
            OCK_RPC_LOG_ERROR("Set worker group option failed, check config:" << OCK_RPC_STR_WRK_THR_GRP << ".");
            return OCK_RPC_ERR;
        }
    }

    iter = configMap.find(OCK_RPC_STR_WRK_CPU_SET);
    if (iter != configMap.end()) {
        ret = strcpy_s(options->workerGroupsCpuSet, NUM_128, iter->second);
        if (ret != OCK_RPC_OK) {
            OCK_RPC_LOG_ERROR("Set worker cpu option failed, check config:" << OCK_RPC_STR_WRK_CPU_SET << ".");
            return OCK_RPC_ERR;
        }
    }

    return OCK_RPC_OK;
}

static int OckRpcRndvReceiveHandler(Service_RndvContext ctx)
{
    int ret;
    Service_RndvMessage rndvMsg;
    ret = Service_GetRndvMessage(ctx, &rndvMsg);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Rndv Get message failed, ret(" << ret << ").");
        return OCK_RPC_ERR;
    }

    OckRpcRndvMessage msg = OckRpcRndvMessage();
    msg.cnt = rndvMsg.cnt;
    for (int i = 0; i < msg.cnt; i++) {
        msg.msg[i].data = rndvMsg.msg[i].data;
        msg.msg[i].len = rndvMsg.msg[i].size;
    }

    Service_RndvType ctxType;
    ret = Service_GetRndvCtxType(ctx, &ctxType);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Rndv Get context type failed, ret(" << ret << ").");
        return OCK_RPC_ERR;
    }

    if (ctxType == C_SER_RNDV_SGL) {
        OCK_RPC_LOG_INFO("Handler receive SGL");
    }

    Service_OpInfo opInfo;
    ret = Service_GetRndvOpInfo(ctx, &opInfo);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Rndv Get option info failed, ret(" << ret << ").");
        return OCK_RPC_ERR;
    }
    serviceAddRndvMutex.lock_shared();

    auto iter = g_rndvHandleMap.find(opInfo.opCode);
    if (iter != g_rndvHandleMap.end()) {
        iter->second(ctx, msg);
    } else {
        OCK_RPC_LOG_INFO("No match msg handle id:" << opInfo.opCode << ", do nothing.");
    }
    serviceAddRndvMutex.unlock();

    return OCK_RPC_OK;
}

static int32_t SetRndvOpHandler(Net_Service service, const std::map<std::string, const char *> &configMap)
{
    auto mallocIter = configMap.find(OCK_RPC_STR_RNDV_MALLOC);
    if (mallocIter == configMap.end()) {
        OCK_RPC_LOG_ERROR("Param key:" << OCK_RPC_STR_RNDV_MALLOC << " not find, check your set.");
        return OCK_RPC_ERR;
    }

    auto freeIter = configMap.find(OCK_RPC_STR_RNDV_MFREE);
    if (freeIter == configMap.end()) {
        OCK_RPC_LOG_ERROR("Param key:" << OCK_RPC_STR_RNDV_MFREE << " not find, check your set.");
        return OCK_RPC_ERR;
    }

    uintptr_t mallocPtr = (uintptr_t)const_cast<char *>(mallocIter->second);
    OckRpcRndvMemAllocate allcFunc = reinterpret_cast<OckRpcRndvMemAllocate>(mallocPtr);
    uintptr_t freePtr = (uintptr_t)const_cast<char *>(freeIter->second);
    OckRpcRndvMemFree freeFunc = reinterpret_cast<OckRpcRndvMemFree>(freePtr);

    Service_RegisterAllocateHandler(service, allcFunc);
    Service_RegisterFreeHandler(service, freeFunc);
    Service_RegisterRndvOpHandler(service, &OckRpcRndvReceiveHandler);

    return OCK_RPC_OK;
}

static int32_t OckRpcDefaultOptionInit(
    std::map<std::string, const char *> &configMap, Service_Options *options, const char *ip)
{
    char ipMask[IP_MAX_LEN] = {0};
    int ret = snprintf_s(ipMask, IP_MAX_LEN, IP_MAX_LEN - 1, "%s/24", ip);
    if (ret < 0) {
        OCK_RPC_LOG_ERROR("Init default option, snprintf ip failed, ret(" << ret << ").");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    // receive buffer config setting
    options->mrSendReceiveSegSize = OCK_MAX_MESSAG_SIZE;
    options->mrSendReceiveSegCount = OCK_MAX_MESSAG_COUNT;

    ret = OckRpcString2Num(configMap, OCK_RPC_STR_PRE_SEGMENT_SIZE, options->mrSendReceiveSegSize);
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    ret = OckRpcString2Num(configMap, OCK_RPC_STR_PRE_SEGMENT_COUNT, options->mrSendReceiveSegCount);
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    // rdma queue size config setting
    options->qpSendQueueSize = OCK_MAX_SEND_QUEUE_SIZE;
    options->qpReceiveQueueSize = OCK_MAX_RECEIVE_QUEUE_SIZE;
    options->prePostReceiveSizePerQP = OCK_MAX_PRE_SEND_RECEIVE_SIZE;
    options->completionQueueDepth = OCK_MAX_COMPLETE_QUEUE_SIZE;

    auto iter = configMap.find(OCK_RPC_STR_SERVER_PORT_RANGE);
    if (iter != configMap.end()) {
        const char *portRange = iter->second;
        ret = memcpy_s(options->oobPortRange, NUM_16, portRange, strlen(portRange));
    } else {
        OCK_RPC_LOG_INFO("Set Default port range 4000-5000");
        ret = memcpy_s(options->oobPortRange, NUM_16, "4000-5000", strlen("4000-5000"));
    }
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Memcpy port range failed, ret:" << ret);
        return OCK_RPC_ERR;
    }

    // limit visitor
    ret = strcpy_s(options->netDeviceIpMask, NUM_256, ipMask);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Set option failed, config ip mask is invalid, check your ip.");
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static int OckRpcSetTlsConfigs(OckRpcServer service, OckRpcCreateConfig *configs,
    std::map<std::string, const char *> &configMap, Service_Options *options)
{
    const char *expectSet = "yes";
    const char *expectSetNo = "no";
    options->enableTls = true;
    options->tlsVersion = C_SERVICE_TLS_1_3;
    auto tlsIter = configMap.find(OCK_RPC_STR_ENABLE_TLS);
    if (tlsIter != configMap.end()) {
        if (!strcmp(tlsIter->second, expectSetNo)) {
            options->enableTls = false;
        } else if (!strcmp(tlsIter->second, expectSet)) {
            options->enableTls = true;
        } else {
            OCK_RPC_LOG_ERROR("Invalid param, OCK_RPC_STR_ENABLE_TLS should be yes or no");
            return OCK_RPC_ERR;
        }
    }
    if ((configs->mask & OCK_RPC_CONFIG_USE_SSL_CALLBACK) && options->enableTls) {
        if (configs->getCaAndVerify == nullptr || configs->getCert == nullptr || configs->getPriKey == nullptr) {
            OCK_RPC_LOG_ERROR("Expand option init failed, tls function is invalid.");
            return OCK_RPC_ERR;
        }
        g_tlsFunc.getCaAndVerify = configs->getCaAndVerify;
        g_tlsFunc.getCert = configs->getCert;
        g_tlsFunc.getPriKey = configs->getPriKey;
        (void)Service_RegisterTLSCb(service, &OckRpcTlsGetCertCb, &OckRpcTlsGetPrivateKeyCb, &OckRpcTlsGetCACb);
        OCK_RPC_LOG_INFO("Expand option init, tls function is registered.");
    } else {
        options->enableTls = false;
    }

    return OCK_RPC_OK;
}

static int32_t OckRpcExpandOptionInit(OckRpcServer service, OckRpcCreateConfig *configs,
    std::map<std::string, const char *> &configMap, Service_Options *options)
{
    int ret = 0;
    if (configs->mask & OCK_RPC_CONFIG_USE_RPC_CONFIGS) {
        ret = OckRpcSetWorkThreadConfigs(configMap, options);
        if (ret != OCK_RPC_OK) {
            return OCK_RPC_ERR;
        }
    }

    uint32_t heartBeatIdleTime = DEFAULT_HEARTBEAT_IDLE_TIME;
    ret = OckRpcString2Num(configMap, OCK_RPC_STR_HEATBEAT_IDLE, heartBeatIdleTime);
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }
    options->heartBeatIdleTime = heartBeatIdleTime;
    OCK_RPC_LOG_INFO("Now Heartbeat idle time is " << options->heartBeatIdleTime);

    options->enableRndv = false;
    auto rndvIter = configMap.find(OCK_RPC_STR_ENABLE_SEND_RNDV);
    const char *expectSet = "yes";
    const char *expectSetNo = "no";
    if (rndvIter != configMap.end()) {
        if (!strcmp(rndvIter->second, expectSet)) {
            ret = SetRndvOpHandler(service, configMap);
            if (ret != OCK_RPC_OK) {
                return OCK_RPC_ERR;
            }
            options->enableRndv = true;
        } else if (!strcmp(rndvIter->second, expectSetNo)) {
            options->enableRndv = false;
        } else {
            OCK_RPC_LOG_ERROR("Invalid param, OCK_RPC_STR_ENABLE_SEND_RNDV should be yes or no");
            return OCK_RPC_ERR;
        }
    }

    ret = OckRpcSetTlsConfigs(service, configs, configMap, options);
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static void HcomLog(int level, const char *msg)
{
    OCK_RPC_HCOM_LOG(level, msg);
}

void OckRpcSetExternalLogger(OckRpcLogHandler h)
{
    Hcom4dbLog::Hcom4dbLog::Instance()->SetExternalLogFunction(h);
    g_logFunc = h;
}

static int32_t OckRpcCheckServerCreateCfg(
    const char *ip, uint16_t port, OckRpcServer *server, OckRpcCreateConfig *configs)
{
    if (ip == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, ip is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (port != 0 && port < OCK_MIN_PORT_NUM) {
        OCK_RPC_LOG_ERROR("Invalid param, port should be 1024 to 65535 or 0.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (server == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, server is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (configs == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, configs is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    return OCK_RPC_OK;
}

OckRpcStatus OckRpcServerCreateWithCfg(const char *ip, uint16_t port, OckRpcServer *server, OckRpcCreateConfig *configs)
{
    int ret = Hcom4dbInitDlopenSo();
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Dlopen so failed");
        return OCK_RPC_ERR;
    }

    if (OckRpcCheckServerCreateCfg(ip, port, server, configs) != OCK_RPC_OK) {
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    std::map<std::string, const char *> configMap;
    ret = OckRpcInitConfigMap(configs, configMap);
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (g_logFunc == nullptr) {
        g_logFunc = &HcomLog;
    }
    Service_SetExternalLogger(g_logFunc);

    ret = OckRpcCreateServer(configMap, server);
    if (ret == OCK_RPC_ERR_INVALID_PARAM) {
        return OCK_RPC_ERR_INVALID_PARAM;
    } else if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    Net_Service service = *server;
    Service_SetOobIpAndPort(service, ip, port);

    OckRpcRegisterChannelHandle(service, configMap);
    Service_RegisterOpHandler(service, 0, C_SERVICE_REQUEST_RECEIVED, &OckRpcReceiveHandle, 1);
    Service_RegisterOpHandler(service, 0, C_SERVICE_REQUEST_POSTED, &OckRpcPostHandle, 1);
    Service_RegisterOpHandler(service, 0, C_SERVICE_READWRITE_DONE, &OckRpcOneSideDone, 1);

    Service_Options *options = &g_options;
    ret = OckRpcDefaultOptionInit(configMap, options, ip);
    if (ret != OCK_RPC_OK) {
        OckRpcServerDestroy(service);
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    ret = OckRpcExpandOptionInit(service, configs, configMap, options);
    if (ret != OCK_RPC_OK) {
        OckRpcServerDestroy(service);
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    OCK_RPC_LOG_INFO("Server create success.");

    return OCK_RPC_OK;
}

bool OckRpcGetOobIpAndPort(OckRpcServer server, char ***ipArray, uint16_t **portArray, int *length)
{
    return Service_GetOobIpAndPort(server, ipArray, portArray, length);
}

OckRpcStatus OckRpcServerAddService(OckRpcServer server, OckRpcService *service)
{
    serviceAddMutex.lock();
    auto iter = std::find_if(g_serviceMap.begin(),
        g_serviceMap.end(),
        [server](const std::pair<std::string, Net_Service> &p) { return p.second == server; });
    if (iter == g_serviceMap.end()) {
        OCK_RPC_LOG_ERROR("Invalid param, server is invalid.");
        serviceAddMutex.unlock();
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (service == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, service is nullptr.");
        serviceAddMutex.unlock();
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (service->id >= MAX_MESSAGE_ID_SIZE) {
        OCK_RPC_LOG_ERROR("Invalid param, service msgId is more than 999.");
        serviceAddMutex.unlock();
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (service->handler == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, service handler is nullptr.");
        serviceAddMutex.unlock();
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    auto mapIter = g_handleMap.find(service->id);
    if (mapIter != g_handleMap.end()) {
        OCK_RPC_LOG_INFO("Service id " << service->id << " exists, will replace old service.");
    }
    g_handleMap[service->id] = service->handler;
    serviceAddMutex.unlock();

    return OCK_RPC_OK;
}

OckRpcStatus OckRpcServerAddRndvService(OckRpcServer server, OckRpcRndvService *service)
{
    serviceAddRndvMutex.lock();
    auto iter = std::find_if(g_serviceMap.begin(),
        g_serviceMap.end(),
        [server](const std::pair<std::string, Net_Service> &p) { return p.second == server; });
    if (iter == g_serviceMap.end()) {
        OCK_RPC_LOG_ERROR("Invalid param, server is invalid.");
        serviceAddRndvMutex.unlock();
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (service == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, service is nullptr.");
        serviceAddRndvMutex.unlock();
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (service->id >= MAX_MESSAGE_ID_SIZE) {
        OCK_RPC_LOG_ERROR("Invalid param, service msgId is more than 999.");
        serviceAddRndvMutex.unlock();
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (service->handler == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, service handler is nullptr.");
        serviceAddRndvMutex.unlock();
        return OCK_RPC_ERR_INVALID_PARAM;
    }
    auto mapIter = g_rndvHandleMap.find(service->id);
    if (mapIter != g_rndvHandleMap.end()) {
        OCK_RPC_LOG_INFO("Service id " << service->id << " exists, will replace old service.");
    }
    g_rndvHandleMap[service->id] = service->handler;
    serviceAddRndvMutex.unlock();

    return OCK_RPC_OK;
}

OckRpcStatus OckRpcServerStart(OckRpcServer server)
{
    int32_t ret = Service_Start(server, g_options);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Server start failed, ret(" << ret << ").");
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

void OckRpcServerDestroy(OckRpcServer server)
{
    serviceMutex.lock();
    auto iter = std::find_if(g_serviceMap.begin(),
        g_serviceMap.end(),
        [server](const std::pair<std::string, Net_Service> &p) { return p.second == server; });
    if (iter == g_serviceMap.end()) {
        OCK_RPC_LOG_ERROR("Invalid server, check your status.");
        serviceMutex.unlock();
        return;
    }

    Service_Stop(server);

    int ret = Service_Destroy(server);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Service destroy failed, ret(" << ret << ")");
        serviceMutex.unlock();
        return;
    }

    g_serviceMap.erase(iter);
    serviceMutex.unlock();
    g_options = Service_Options();
    g_tlsFunc = TlsFuncs();
}

OckRpcServerContext OckRpcCloneCtx(OckRpcServerContext ctx)
{
    return Service_ContextClone(ctx);
}

void OckRpcDeCloneCtx(OckRpcServerContext ctx)
{
    Service_ContextDeClone(ctx);
}

void OckRpcServerCleanupCtx(OckRpcServerContext ctx)
{
    return;
}

static int OckRpcGetCurService(std::map<std::string, const char *> &configMap, Net_Service *service)
{
    std::string serviceName = "service";
    auto iter = configMap.find(OCK_RPC_STR_SERVER_NAME);
    if (iter != configMap.end()) {
        serviceName = iter->second;
    }
    if (serviceName.size() > NUM_64) {
        OCK_RPC_LOG_ERROR("Invalid serviceName, bigger than 64");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    serviceMutex.lock_shared();
    auto iterService = g_serviceMap.find(serviceName);
    if (iterService == g_serviceMap.end()) {
        OCK_RPC_LOG_ERROR("Connect failed, check whether the value of OCK_RPC_STR_SERVER_NAME is the same as that of "
                          "the created server.");
        serviceMutex.unlock_shared();
        return OCK_RPC_ERR;
    }
    serviceMutex.unlock_shared();

    *service = iterService->second;

    return OCK_RPC_OK;
}

static int OckRpcSetConnectionOption(std::map<std::string, const char *> &configMap, Service_ConnectOpt *connOption)
{
    connOption->epSize = DEFAULT_CONNECTION_EP_SIZE;
    connOption->flags = SER_C_CHANNEL_SELF_POLLING;

    const char *expectType = "no";
    const char *expectTypeYes = "yes";
    auto pollingTypeIter = configMap.find(OCK_RPC_STR_ENABLE_SELFPOLLING);
    if (pollingTypeIter != configMap.end()) {
        if (!strcmp(pollingTypeIter->second, expectType)) {
            connOption->flags = 0;
            OCK_RPC_LOG_INFO("Set worker polling");
        } else if (!strcmp(pollingTypeIter->second, expectTypeYes)) {
            connOption->flags = SER_C_CHANNEL_SELF_POLLING;
            OCK_RPC_LOG_INFO("Set self polling");
        } else {
            OCK_RPC_LOG_ERROR("Invalid param, OCK_RPC_STR_ENABLE_SELFPOLLING should be yes or no");
            return OCK_RPC_ERR;
        }
    } else {
        OCK_RPC_LOG_INFO("Set self polling");
    }

    return OCK_RPC_OK;
}

static int32_t OckRpcCheckClientCreateCfg(
    const char *ip, uint16_t port, OckRpcClient *client, OckRpcCreateConfig *configs)
{
    if (ip == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, ip is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (port != 0 && port < OCK_MIN_PORT_NUM) {
        OCK_RPC_LOG_ERROR("Invalid param, port should be 1024 to 65535 or 0.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (client == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, client is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (configs == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, configs is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    return OCK_RPC_OK;
}

OckRpcStatus OckRpcClientConnectWithCfg(
    const char  *ip, uint16_t port, OckRpcClient *client, OckRpcCreateConfig *configs)
{
    if (OckRpcCheckClientCreateCfg(ip, port, client, configs) != OCK_RPC_OK) {
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    int ret;
    std::map<std::string, const char *> configMap;
    ret = OckRpcInitConfigMap(configs, configMap);
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    Net_Service service = 0;
    ret = OckRpcGetCurService(configMap, &service);
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    const char *payLoad = "hello";
    auto payLoadIter = configMap.find(OCK_RPC_STR_CLIENT_CREATE_INFO);
    if (payLoadIter != configMap.end()) {
        payLoad = payLoadIter->second;
    }

    Service_ConnectOpt connOption = Service_ConnectOpt();
    ret = OckRpcSetConnectionOption(configMap, &connOption);
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    ret = Service_Connect(service, ip, port, payLoad, client, &connOption);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Connect failed, ret(" << ret << ").");
        return OCK_RPC_ERR;
    }

    ret = OckRpcPrepareMemAllocator(service, *client, configMap);
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

void OckRpcClientDisconnect(OckRpcClient client)
{
    int ret = 0;
    auto iter = g_memoryAllocator.find(client);
    if (iter != g_memoryAllocator.end()) {
        ret = Net_MemoryAllocatorDestroy(iter->second);
        if (ret != OCK_RPC_OK) {
            OCK_RPC_LOG_ERROR("Disconnect destroy allocator failed.");
        }
        g_memoryAllocator.erase(client);
    }

    auto iterMr = g_memoryRegion.find(client);
    if (iterMr != g_memoryRegion.end()) {
        OckRpcMrInfo mrInfo = iterMr->second;
        std::free(reinterpret_cast<void *>(mrInfo.mrInfo.lAddress));
        Service_DestroyMemoryRegion(mrInfo.server, mrInfo.mr);
        g_memoryRegion.erase(client);
    }

    Channel_Close(client);
}

void OckRpcClientDisconnectBreakHandler(OckRpcClient client)
{
    Channel_DeRefer(client);
}

int OckRpcSetUpCtx(OckRpcClient client, uint64_t ctx)
{
    return Channel_SetUpCtx(client, ctx);
}

int OckRpcGetUpCtx(OckRpcClient client, uint64_t *ctx)
{
    return Channel_GetUpCtx(client, ctx);
}

static void ResponseCallBack(void *arg, Service_Context context)
{
    AsyncCbInfo *cbInfo = (AsyncCbInfo *)arg;
    OckRpcDoneCallback cb = cbInfo->cb;
    void *doneArg = cbInfo->arg;
    if (cb != nullptr) {
        int result = OCK_RPC_OK;
        if (Service_GetResult(context, &result) != OCK_RPC_OK) {
            OCK_RPC_LOG_ERROR("Reply call async failed:" << result);
        }
        cb(static_cast<OckRpcStatus>(result), doneArg);
    }
    std::free(cbInfo);
    return;
}

OckRpcStatus OckRpcServerReply(OckRpcServerContext ctx, uint16_t msgId, OckRpcMessage *reply, OckRpcCallDone *done)
{
    if (reply == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, reply is nullptr.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (reply->len > UINT32_MAX) {
        OCK_RPC_LOG_ERROR("Invalid param, reply len should be less than UINT32_MAX.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    Net_Channel channel;
    Service_RspCtx rspCtx;
    Service_ContextType ctxType;
    if (Service_GetChannel(ctx, &channel) != OCK_RPC_OK || Service_GetRspCtx(ctx, &rspCtx) != OCK_RPC_OK ||
        Service_GetContextType(ctx, &ctxType) != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Reply get info with ctx failed");
        return OCK_RPC_ERR;
    }

    Service_OpInfo rspOpInfo = Service_OpInfo();

    AsyncCbInfo *cbInfo = nullptr;
    cbInfo = (AsyncCbInfo *)std::malloc(sizeof(AsyncCbInfo));
    if (cbInfo == nullptr) {
        OCK_RPC_LOG_ERROR("Malloc call back info size " << sizeof(AsyncCbInfo) << " failed");
        return OCK_RPC_ERR;
    }
    if (done != nullptr) {
        cbInfo->cb = done->cb;
        cbInfo->arg = done->arg;
    } else {
        cbInfo->cb = nullptr;
        cbInfo->arg = nullptr;
    }

    Channel_Callback rspCb = {.cb = ResponseCallBack, .arg = static_cast<void *>(cbInfo)};
    Service_Message req;
    req.data = reply->data;
    req.size = reply->len;

    int ret = 0;
    if (ctxType == SERVICE_RECEIVED_RAW) {
        ret = Channel_PostResponseRaw(channel, rspCtx, &req, &rspCb);
    } else {
        ret = Channel_PostResponse(channel, rspCtx, &rspOpInfo, &req, &rspCb);
    }
    if (ret != 0) {
        free(cbInfo);
        OCK_RPC_LOG_ERROR("Reply failed, ret(" << ret << ")");
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

static void OckRpcCallbackFunc(void *arg, Service_Context context)
{
    int result = OCK_RPC_OK;
    Service_ContextType cbType;
    if (Service_GetResult(context, &result) != OCK_RPC_OK || Service_GetContextType(context, &cbType) != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("CallBack get failed.");
        std::free(arg);
        return;
    }

    if (cbType == SERVICE_ONE_SIDE) {
        AsyncCbInfo *cbInfo = (AsyncCbInfo *)arg;
        cbInfo->cb(static_cast<OckRpcStatus>(result), cbInfo->arg);
        std::free(cbInfo);
        return;
    }

    AsyncCbInfo *cbInfo = (AsyncCbInfo *)arg;
    OckRpcDoneCallback cb = cbInfo->cb;
    void *doneArg = cbInfo->arg;
    OckRpcClient client = cbInfo->client;
    uintptr_t *reqAddr = cbInfo->reqAddr;
    uint32_t reqCnt = cbInfo->reqCnt;
    OckRpcMessage *rspMsg = cbInfo->rsp;
    std::free(cbInfo);
    int ret = 0;
    if (reqAddr != nullptr) {
        for (uint32_t i = 0; i < reqCnt; i++) {
            OckRpcMemFree(client, reqAddr[i]);
        }
        std::free(reqAddr);
    }

    if (rspMsg != nullptr) {
        rspMsg->len = Service_GetMessageDataLen(context);
        if (rspMsg->data == nullptr) {
            rspMsg->data = malloc(rspMsg->len);
        }
        ret = memcpy_s(rspMsg->data, rspMsg->len, Service_GetMessageData(context), Service_GetMessageDataLen(context));
        if (ret != OCK_RPC_OK) {
            OCK_RPC_LOG_ERROR(
                "Memcpy response data failed, ret:" << ret << ", rsp->data:" << rspMsg->data << ", len:" << rspMsg->len);
        }
    }

    if (cb == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid cb nullptr.");
        return;
    }

    cb(static_cast<OckRpcStatus>(result), doneArg);
}

static int32_t OckRpcCallSync(
    OckRpcClient client, Service_OpInfo *reqOpInfo, Service_Message *request, OckRpcMessage *response)
{
    int ret;
    if (response != nullptr) {
        Service_OpInfo rspOpInfo = Service_OpInfo();
        Service_Message rsp = {.data = response->data, .size = static_cast<uint32_t>(response->len)};
        ret = Channel_SyncCall(client, reqOpInfo, request, &rspOpInfo, &rsp);
        if (ret != OCK_RPC_OK) {
            OCK_RPC_LOG_ERROR("OckRpcCallSync needs response failed, ret(" << ret << ").");
            return OCK_RPC_ERR;
        }
        response->data = rsp.data;
        response->len = rsp.size;
    } else {
        ret = Channel_PostSend(client, reqOpInfo, request, nullptr);
        if (ret != OCK_RPC_OK) {
            OCK_RPC_LOG_ERROR("OckRpcCallSync ignores response failed, ret(" << ret << ").");
            return OCK_RPC_ERR;
        }
    }

    return OCK_RPC_OK;
}

static int32_t OckRpcCallASync(OckRpcClient client, Service_OpInfo *reqOpInfo, Service_Message *request,
    OckRpcMessage *response, OckRpcCallDone *done)
{
    int ret;
    AsyncCbInfo *cbInfo = nullptr;
    cbInfo = (AsyncCbInfo *)std::malloc(sizeof(AsyncCbInfo));
    if (cbInfo == nullptr) {
        OCK_RPC_LOG_ERROR("Malloc call back info size " << sizeof(AsyncCbInfo) << " failed");
        return OCK_RPC_ERR;
    }
    cbInfo->cb = done->cb;
    cbInfo->arg = done->arg;
    cbInfo->reqAddr = nullptr;
    cbInfo->reqCnt = 0;
    cbInfo->rsp = response;
    Channel_Callback cb = {.cb = OckRpcCallbackFunc, .arg = static_cast<void *>(cbInfo)};
    if (response != nullptr) {
        ret = Channel_AsyncCall(client, reqOpInfo, request, &cb);
        if (ret != OCK_RPC_OK) {
            std::free(cbInfo);
            OCK_RPC_LOG_ERROR("OckRpcCallAsync needs response failed, ret(" << ret << ").");
            return OCK_RPC_ERR;
        }
    } else {
        ret = Channel_PostSend(client, reqOpInfo, request, &cb);
        if (ret != OCK_RPC_OK) {
            std::free(cbInfo);
            OCK_RPC_LOG_ERROR("OckRpcCallAsync ignores response failed, ret(" << ret << ").");
            return OCK_RPC_ERR;
        }
    }

    return OCK_RPC_OK;
}

OckRpcStatus OckRpcClientCall(
    OckRpcClient client, uint16_t msgId, OckRpcMessage *request, OckRpcMessage *response, OckRpcCallDone *done)
{
    if (request == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, request is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (request->len > UINT32_MAX) {
        OCK_RPC_LOG_ERROR("Invalid param, request len should be less than UINT32_MAX.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    int32_t ret = 0;
    Service_OpInfo reqOpInfo = Service_OpInfo();
    reqOpInfo.opCode = msgId;
    reqOpInfo.timeout = g_timeoutTime;
    Service_Message req = {.data = request->data, .size = static_cast<uint32_t>(request->len)};

    if (done == nullptr) {
        ret = OckRpcCallSync(client, &reqOpInfo, &req, response);
        if (ret != OCK_RPC_OK) {
            return OCK_RPC_ERR;
        }
    } else {
        ret = OckRpcCallASync(client, &reqOpInfo, &req, response, done);
        if (ret != OCK_RPC_OK) {
            return OCK_RPC_ERR;
        }
    }

    return OCK_RPC_OK;
}

static int32_t OckRpcCallSyncSgl(OckRpcClient client, Service_SglRequest *request, OckRpcMessage *response)
{
    int ret = 0;
    if (response != nullptr) {
        Service_Message rsp = {.data = response->data, .size = static_cast<uint32_t>(response->len)};
        ret = Channel_SyncCallRawSgl(client, request, &rsp);
        if (ret != OCK_RPC_OK) {
            OCK_RPC_LOG_ERROR("OckRpcCallSyncSgl needs response failed, ret(" << ret << ").");
            return OCK_RPC_ERR;
        }
        response->data = rsp.data;
        response->len = rsp.size;
    } else {
        ret = Channel_PostSendRawSgl(client, request, nullptr);
        if (ret != OCK_RPC_OK) {
            OCK_RPC_LOG_ERROR("OckRpcCallSyncSgl ignores response failed, ret(" << ret << ").");
            return OCK_RPC_ERR;
        }
    }

    return OCK_RPC_OK;
}

static int32_t OckRpcCallAsyncSgl(
    OckRpcClient client, Service_SglRequest *request, OckRpcMessage *response, OckRpcCallDone *done)
{
    int ret;
    AsyncCbInfo *cbInfo = nullptr;
    cbInfo = (AsyncCbInfo *)std::malloc(sizeof(AsyncCbInfo));
    if (cbInfo == nullptr) {
        OCK_RPC_LOG_ERROR("Malloc call back info size " << sizeof(AsyncCbInfo) << " failed");
        return OCK_RPC_ERR;
    }

    cbInfo->client = client;
    cbInfo->cb = done->cb;
    cbInfo->arg = done->arg;
    cbInfo->reqCnt = request->iovCount;
    cbInfo->reqAddr = nullptr;
    cbInfo->reqAddr = (uintptr_t *)std::malloc(sizeof(uintptr_t) * cbInfo->reqCnt);
    if (cbInfo->reqAddr == nullptr) {
        OCK_RPC_LOG_ERROR("Malloc call back info size " << sizeof(uintptr_t) * cbInfo->reqCnt << " failed");
        free(cbInfo);
        return OCK_RPC_ERR;
    }

    for (uint32_t i = 0; i < cbInfo->reqCnt; i++) {
        cbInfo->reqAddr[i] = request->iov[i].lAddress;
    }
    cbInfo->rsp = response;
    Channel_Callback cb = {.cb = OckRpcCallbackFunc, .arg = static_cast<void *>(cbInfo)};
    if (response != nullptr) {
        ret = Channel_AsyncCallRawSgl(client, request, &cb);
        if (ret != OCK_RPC_OK) {
            free(cbInfo->reqAddr);
            free(cbInfo);
            OCK_RPC_LOG_ERROR("OckRpcCallAsyncSgl needs response failed, ret(" << ret << ").");
            return OCK_RPC_ERR;
        }
    } else {
        ret = Channel_PostSendRawSgl(client, request, &cb);
        if (ret != OCK_RPC_OK) {
            free(cbInfo->reqAddr);
            free(cbInfo);
            OCK_RPC_LOG_ERROR("OckRpcCallAsyncSgl ignores response failed, ret(" << ret << ").");
            return OCK_RPC_ERR;
        }
    }

    return OCK_RPC_OK;
}

static int32_t SetReqInfo(OckRpcClient client, OckRpcMessageIov *reqIov, uint32_t count, Service_SglRequest *serviceReq)
{
    int ret = OCK_RPC_OK;
    uintptr_t addrArr[count] = {};
    uint32_t keyArr[count] = {};
    uint32_t size = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t reqPos = i + 1;
        size = (i == count - 1) ? g_options.mrSendReceiveSegSize : reqIov->msgs[i].len;
        ret = OckRpcMemAlloc(client, size, &addrArr[i], &keyArr[i]);
        if (ret != OCK_RPC_OK) {
            OCK_RPC_LOG_ERROR("Alloc failed, message id " << i << " size " << size << " is too large.");
            for (uint32_t j = 0; j < i; j++) {
                OckRpcMemFree(client, addrArr[i]);
            }
            return OCK_RPC_ERR;
        }
        ret = memcpy_s(reinterpret_cast<void *>(addrArr[i]),
            g_options.mrSendReceiveSegSize,
            reqIov->msgs[i].data,
            reqIov->msgs[i].len);
        if (ret != OCK_RPC_OK) {
            OCK_RPC_LOG_ERROR("Memcpy multi data failed, ret(" << ret << ").");
            for (uint32_t j = 0; j <= i; j++) {
                OckRpcMemFree(client, addrArr[i]);
            }
            return OCK_RPC_ERR;
        }

        serviceReq->iovCount++;
        serviceReq->iov[reqPos].lAddress = addrArr[i];
        serviceReq->iov[reqPos].lKey = keyArr[i];
        serviceReq->iov[reqPos].size = reqIov->msgs[i].len;
    }

    return OCK_RPC_OK;
}

static void FreeAllMemory(OckRpcClient client, const Service_SglRequest *serviceReq)
{
    for (uint32_t i = 0; i < OCK_MAX_IOV_COUNT; i++) {
        OckRpcMemFree(client, serviceReq->iov[i].lAddress);
    }
}

static int32_t PackReqInfo(
    OckRpcClient client, OckRpcMessageIov *reqIov, uint16_t msgId, Service_SglRequest *serviceReq)
{
    int32_t ret = OCK_RPC_OK;
    uint32_t curSglReqPos = 0;
    uintptr_t headAddr = 0;
    uint32_t headKey = 0;
    ret = OckRpcMemAlloc(client, sizeof(RawMessageHeader), &headAddr, &headKey);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Pack, message header allocate failed.");
        return OCK_RPC_ERR;
    }
    auto* head = reinterpret_cast<RawMessageHeader*>(headAddr);
    head->msgId = msgId;
    serviceReq->iovCount++;
    serviceReq->iov[curSglReqPos].lAddress = headAddr;
    serviceReq->iov[curSglReqPos].lKey = headKey;
    serviceReq->iov[curSglReqPos].size = sizeof(RawMessageHeader);
    curSglReqPos += 1;

    uint32_t maxCount = OCK_MAX_IOV_COUNT - curSglReqPos;
    if (reqIov->count > maxCount) {
        ret = SetReqInfo(client, reqIov, maxCount, serviceReq);
        if (ret != OCK_RPC_OK) {
            OckRpcMemFree(client, headAddr);
            return OCK_RPC_ERR;
        }
        for (uint32_t i = maxCount; i < reqIov->count; i++) {
            ret = memcpy_s(
                reinterpret_cast<uint8_t *>(serviceReq->iov[maxCount].lAddress) + serviceReq->iov[maxCount].size,
                g_options.mrSendReceiveSegSize - serviceReq->iov[maxCount].size,
                reqIov->msgs[i].data,
                reqIov->msgs[i].len);
            if (ret != OCK_RPC_OK) {
                OCK_RPC_LOG_ERROR("Memcpy append multi data faild, ret(" << ret << ").");
                FreeAllMemory(client, serviceReq);
                return OCK_RPC_ERR;
            }
            serviceReq->iov[maxCount].size += reqIov->msgs[i].len;
        }
    } else {
        ret = SetReqInfo(client, reqIov, reqIov->count, serviceReq);
        if (ret != OCK_RPC_OK) {
            FreeAllMemory(client, serviceReq);
            return OCK_RPC_ERR;
        }
    }

    return OCK_RPC_OK;
}

OckRpcStatus OckRpcClientCallWithParam(OckRpcClientCallParams *params)
{
    if (params == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, params is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (params->reqIov.msgs == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, params reqIov.msgs is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (params->reqIov.count > MAX_CALL_WITH_PARAMS_COUNT || params->reqIov.count <= 0) {
        OCK_RPC_LOG_ERROR("Invalid param, params messages count should be [1, 8].");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    for (size_t i = 0; i < params->reqIov.count; i++) {
        if (params->reqIov.msgs[i].len > g_options.mrSendReceiveSegSize) {
            OCK_RPC_LOG_ERROR(
                "Invalid param, params messages count should be less than setting:OCK_RPC_STR_PRE_SEGMENT_SIZE");
            return OCK_RPC_ERR_INVALID_PARAM;
        }
    }

    int ret = OCK_RPC_OK;
    OckRpcMessageIov *reqIov = &params->reqIov;
    Service_SglRequest serviceReq = Service_SglRequest();
    Service_Request iovArr[OCK_MAX_IOV_COUNT] = {Service_Request()};
    serviceReq.iov = iovArr;

    ret = PackReqInfo(params->client, reqIov, params->msgId, &serviceReq);
    if (ret != OCK_RPC_OK) {
        return OCK_RPC_ERR;
    }

    OckRpcMessage *response = params->rspIov.msgs;
    if (params->done == nullptr) {
        ret = OckRpcCallSyncSgl(params->client, &serviceReq, response);
        for (uint32_t i = 0; i < serviceReq.iovCount; i++) {
            OckRpcMemFree(params->client, serviceReq.iov[i].lAddress);
        }
        if (ret != OCK_RPC_OK) {
            return OCK_RPC_ERR;
        }
    } else {
        ret = OckRpcCallAsyncSgl(params->client, &serviceReq, response, params->done);
        if (ret != OCK_RPC_OK) {
            return OCK_RPC_ERR;
        }
    }

    return OCK_RPC_OK;
}

int OckRpcMemoryAllocatorCreate(
    OckRpcMemoryAllocatorType t, OckRpcMemoryAllocatorOptions *options, OckRpcMemoryAllocator *allocator)
{
    Net_MemoryAllocatorType type = Net_MemoryAllocatorType();
    if (t == MEM_DYNAMIC_SIZE) {
        type = C_DYNAMIC_SIZE;
    } else if (t == MEM_DYNAMIC_SIZE_WITH_CACHE) {
        type = C_DYNAMIC_SIZE_WITH_CACHE;
    }

    if (options == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, option is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    Net_MemoryAllocatorOptions memOptions = Net_MemoryAllocatorOptions();
    memOptions.address = options->address;
    memOptions.alignedAddress = options->alignedAddress;
    memOptions.size = options->size;
    memOptions.minBlockSize = options->minBlockSize;
    memOptions.bucketCount = options->bucketCount;
    memOptions.cacheBlockCountPerTier = options->cacheBlockCountPerTier;
    if (options->cacheTierPolicy == MEM_TIER_TIMES) {
        memOptions.cacheTierPolicy = C_TIER_TIMES;
    } else if (options->cacheTierPolicy == MEM_TIER_POWER) {
        memOptions.cacheTierPolicy = C_TIER_POWER;
    }
    memOptions.cacheTierCount = options->cacheTierCount;

    return Net_MemoryAllocatorCreate(type, &memOptions, allocator);
}

int OckRpcMemoryAllocatorDestroy(OckRpcMemoryAllocator allocator)
{
    return Net_MemoryAllocatorDestroy(allocator);
}

int OckRpcMemoryAllocatorSetMrKey(OckRpcMemoryAllocator allocator, uint32_t mrKey)
{
    return Net_MemoryAllocatorSetMrKey(allocator, mrKey);
}

int OckRpcMemoryAllocatorMemOffset(OckRpcMemoryAllocator allocator, uintptr_t address, uintptr_t *offset)
{
    return Net_MemoryAllocatorMemOffset(allocator, address, offset);
}

int OckRpcMemoryAllocatorFreeSize(OckRpcMemoryAllocator allocator, uintptr_t *size)
{
    return Net_MemoryAllocatorFreeSize(allocator, size);
}

int OckRpcMemoryAllocatorAllocate(OckRpcMemoryAllocator allocator, uint64_t size, uintptr_t *address, uint32_t *key)
{
    return Net_MemoryAllocatorAllocate(allocator, size, address, key);
}

int OckRpcMemoryAllocatorFree(OckRpcMemoryAllocator allocator, uintptr_t address)
{
    return Net_MemoryAllocatorFree(allocator, address);
}

void OckRpcDisableSecureHmac(void)
{
    // 多余功能，直接返回
    return;
}

OckRpcStatus OckRpcServerReplyRndv(
    OckRpcServerRndvContext ctx, uint16_t msgId, OckRpcMessage *reply, OckRpcCallDone *done)
{
    if (reply == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, reply is nullptr.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (reply->len > UINT32_MAX) {
        OCK_RPC_LOG_ERROR("Invlid param, rndv reply len should be less than UINT32_MAX.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    Service_OpInfo rspOpInfo = Service_OpInfo();

    AsyncCbInfo *cbInfo = nullptr;
    cbInfo = (AsyncCbInfo *)std::malloc(sizeof(AsyncCbInfo));
    if (cbInfo == nullptr) {
        OCK_RPC_LOG_ERROR("Malloc call back info size " << sizeof(AsyncCbInfo) << " failed");
        return OCK_RPC_ERR;
    }
    if (done != nullptr) {
        cbInfo->cb = done->cb;
        cbInfo->arg = done->arg;
    } else {
        cbInfo->cb = nullptr;
        cbInfo->arg = nullptr;
    }
    Channel_Callback rspCb = {.cb = ResponseCallBack, .arg = static_cast<void *>(cbInfo)};
    Service_Message rsp;
    rsp.data = reply->data;
    rsp.size = reply->len;

    int32_t ret = Service_RndvReply(ctx, &rspOpInfo, &rsp, &rspCb);
    if (ret != 0) {
        OCK_RPC_LOG_ERROR("Reply failed, ret(" << ret << ").");
        free(cbInfo);
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

OckRpcStatus OckRpcClientCallRndv(
    OckRpcClient client, uint16_t msgId, OckRpcMessageInfo *request, OckRpcMessage *response, OckRpcCallDone *done)
{
    if (request == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, request is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    if (response == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, response is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    int32_t ret = 0;
    Service_OpInfo reqOpInfo = Service_OpInfo();
    reqOpInfo.opCode = msgId;
    reqOpInfo.timeout = g_timeoutTime;

    Service_Request req = Service_Request();
    req.lAddress = request->lAddress;
    req.lKey = request->lKey;
    req.size = request->size;
    if (done == nullptr) {
        Service_OpInfo rspOpInfo = Service_OpInfo();
        Service_Message rsp = {.data = response->data, .size = static_cast<uint32_t>(response->len)};
        ret = Channel_SyncRndvCall(client, &reqOpInfo, &req, &rspOpInfo, &rsp);
        if (ret != OCK_RPC_OK) {
            return OCK_RPC_ERR;
        }
        response->data = rsp.data;
        response->len = rsp.size;
    } else {
        AsyncCbInfo *cbInfo = nullptr;
        cbInfo = (AsyncCbInfo *)std::malloc(sizeof(AsyncCbInfo));
        if (cbInfo == nullptr) {
            OCK_RPC_LOG_ERROR("Malloc call back info size " << sizeof(AsyncCbInfo) << " failed");
            return OCK_RPC_ERR;
        }
        cbInfo->cb = done->cb;
        cbInfo->arg = done->arg;
        cbInfo->reqAddr = nullptr;
        cbInfo->reqCnt = 0;
        cbInfo->rsp = response;
        Channel_Callback cb = {.cb = OckRpcCallbackFunc, .arg = static_cast<void *>(cbInfo)};
        ret = Channel_AsyncRndvCall(client, &reqOpInfo, &req, &cb);
        if (ret != OCK_RPC_OK) {
            free(cbInfo);
            return OCK_RPC_ERR;
        }
    }
    return OCK_RPC_OK;
}

static int OckRpcRndvIovCheckParams(OckRpcMessageInfoIov *request, OckRpcMessage *response)
{
    if (request == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, request is null.");
        return OCK_RPC_ERR;
    }

    if (response == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, response is null.");
        return OCK_RPC_ERR;
    }

    if (request->count > MAX_MESSAGE_SGL_COUNT || request->count <= 0) {
        OCK_RPC_LOG_ERROR("Invalid param, params messages len should be [1, 4]");
        return OCK_RPC_ERR;
    }

    if (request->reqs == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, reqs is null.");
        return OCK_RPC_ERR;
    }

    return OCK_RPC_OK;
}

OckRpcStatus OckRpcClientCallRndvIov(
    OckRpcClient client, uint16_t msgId, OckRpcMessageInfoIov *request, OckRpcMessage *response, OckRpcCallDone *done)
{
    if (OckRpcRndvIovCheckParams(request, response) != OCK_RPC_OK) {
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    Service_OpInfo reqOpInfo = Service_OpInfo();
    reqOpInfo.opCode = msgId;

    Service_SglRequest req;
    req.iovCount = request->count;
    Service_Request msg[req.iovCount] = {Service_Request()};
    for (int i = 0; i < req.iovCount; i++) {
        msg[i].lAddress = request->reqs[i].lAddress;
        msg[i].lKey = request->reqs[i].lKey;
        msg[i].size = request->reqs[i].size;
    }
    req.iov = msg;

    if (done == nullptr) {
        Service_OpInfo rspOpInfo = Service_OpInfo();
        Service_Message rsp = {.data = response->data, .size = static_cast<uint32_t>(response->len)};
        int ret = Channel_SyncRndvSglCall(client, &reqOpInfo, &req, &rspOpInfo, &rsp);
        if (ret != OCK_RPC_OK) {
            return OCK_RPC_ERR;
        }
        response->data = rsp.data;
        response->len = rsp.size;
    } else {
        AsyncCbInfo *cbInfo = nullptr;
        cbInfo = (AsyncCbInfo *)std::malloc(sizeof(AsyncCbInfo));
        if (cbInfo == nullptr) {
            OCK_RPC_LOG_ERROR("Malloc call back info size " << sizeof(AsyncCbInfo) << " failed");
            return OCK_RPC_ERR;
        }
        cbInfo->cb = done->cb;
        cbInfo->arg = done->arg;
        cbInfo->reqAddr = nullptr;
        cbInfo->reqCnt = 0;
        cbInfo->rsp = response;
        Channel_Callback cb = {.cb = OckRpcCallbackFunc, .arg = static_cast<void *>(cbInfo)};
        int ret = Channel_AsyncRndvSglCall(client, &reqOpInfo, &req, &cb);
        if (ret != OCK_RPC_OK) {
            free(cbInfo);
            return OCK_RPC_ERR;
        }
    }
    return OCK_RPC_OK;
}

void OckRpcClientSetTimeout(OckRpcClient client, int64_t timeout)
{
    int32_t timeoutSeconds = 0;
    if (timeout < 0) {
        timeoutSeconds = timeout;
    } else {
        timeoutSeconds = static_cast<int32_t>(timeout / MILLOSECOND_PER_SECOND);
    }
    g_timeoutTime = timeoutSeconds;
    Channel_SetOneSideTimeout(client, g_timeoutTime);
    Channel_SetTwoSideTimeout(client, g_timeoutTime);
    OCK_RPC_LOG_INFO("Set timeout " << g_timeoutTime << " s");
}

int OckRpcGetClient(OckRpcServerContext ctx, OckRpcClient *client)
{
    return Service_GetChannel(ctx, client);
}

int OckRpcGetRndvClient(OckRpcServerRndvContext ctx, OckRpcClient *client)
{
    return Service_GetRndvChannel(ctx, client);
}

int OckRpcServerCleanupRndvCtx(OckRpcServerRndvContext ctx)
{
    return Service_RndvFreeContext(ctx);
}

int OckRpcRegisterMemoryRegion(OckRpcServer server, uint64_t size, OckRpcMemoryRegion *mr)
{
    return Service_RegisterMemoryRegion(server, size, mr);
}

int OckRpcRegisterAssignMemoryRegion(OckRpcServer server, uintptr_t address, uint64_t size, OckRpcMemoryRegion *mr)
{
    return Service_RegisterAssignMemoryRegion(server, address, size, mr);
}

int OckRpcGetMemoryRegionInfo(OckRpcMemoryRegion mr, OckRpcMemoryRegionInfo *info)
{
    if (info == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, info is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    Service_MemoryRegionInfo serviceInfo = Service_MemoryRegionInfo();
    int ret = Service_GetMemoryRegionInfo(mr, &serviceInfo);
    if (ret != OCK_RPC_OK) {
        OCK_RPC_LOG_ERROR("Hcom service get memory region failed, ret:" << ret);
        return OCK_RPC_ERR;
    }
    info->lAddress = serviceInfo.lAddress;
    info->lKey = serviceInfo.lKey;
    info->size = serviceInfo.size;

    return OCK_RPC_OK;
}

void OckRpcDestroyMemoryRegion(OckRpcServer server, OckRpcMemoryRegion mr)
{
    Service_DestroyMemoryRegion(server, mr);
}

int OckRpcRead(OckRpcClient client, OckRpcRequest *req, OckRpcCallDone *cb)
{
    if (req == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, req is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    Service_Request reqMsg = Service_Request();
    reqMsg.lAddress = req->lAddress;
    reqMsg.rAddress = req->rAddress;
    reqMsg.lKey = req->lKey;
    reqMsg.rKey = req->rKey;
    reqMsg.size = req->size;

    if (cb == nullptr) {
        return Channel_Read(client, &reqMsg, nullptr);
    }

    AsyncCbInfo *cbInfo = nullptr;
    cbInfo = reinterpret_cast<AsyncCbInfo *>(malloc(sizeof(AsyncCbInfo)));
    if (cbInfo == nullptr) {
        OCK_RPC_LOG_ERROR("Read malloc call back info size " << sizeof(AsyncCbInfo) << " failed");
        return OCK_RPC_ERR;
    }
    cbInfo->cb = cb->cb;
    cbInfo->arg = cb->arg;
    Channel_Callback callBack = {.cb = OckRpcCallbackFunc, .arg = static_cast<void *>(cbInfo)};
    int ret = 0;
    ret = Channel_Read(client, &reqMsg, &callBack);
    if (ret != OCK_RPC_OK) {
        free(cbInfo);
        return ret;
    }

    return OCK_RPC_OK;
}

int OckRpcReadSgl(OckRpcClient client, OckRpcSglRequest *req, OckRpcCallDone *cb)
{
    if (cb == nullptr) {
        return Channel_ReadSgl(client, reinterpret_cast<Service_SglRequest *>(req), nullptr);
    }

    AsyncCbInfo *cbInfo = nullptr;
    cbInfo = reinterpret_cast<AsyncCbInfo *>(malloc(sizeof(AsyncCbInfo)));
    if (cbInfo == nullptr) {
        OCK_RPC_LOG_ERROR("ReadSgl malloc call back info size " << sizeof(AsyncCbInfo) << " failed");
        return OCK_RPC_ERR;
    }
    cbInfo->cb = cb->cb;
    cbInfo->arg = cb->arg;
    Channel_Callback callBack = {.cb = OckRpcCallbackFunc, .arg = static_cast<void *>(cbInfo)};
    int ret = 0;
    ret = Channel_ReadSgl(client, reinterpret_cast<Service_SglRequest *>(req), &callBack);
    if (ret != OCK_RPC_OK) {
        free(cbInfo);
        return ret;
    }

    return OCK_RPC_OK;
}

int OckRpcWrite(OckRpcClient client, OckRpcRequest *req, OckRpcCallDone *cb)
{
    if (req == nullptr) {
        OCK_RPC_LOG_ERROR("Invalid param, req is null.");
        return OCK_RPC_ERR_INVALID_PARAM;
    }

    Service_Request reqMsg = Service_Request();
    reqMsg.lAddress = req->lAddress;
    reqMsg.rAddress = req->rAddress;
    reqMsg.lKey = req->lKey;
    reqMsg.rKey = req->rKey;
    reqMsg.size = req->size;
    if (cb == nullptr) {
        return Channel_Write(client, &reqMsg, nullptr);
    }

    AsyncCbInfo *cbInfo = nullptr;
    cbInfo = reinterpret_cast<AsyncCbInfo *>(malloc(sizeof(AsyncCbInfo)));
    if (cbInfo == nullptr) {
        OCK_RPC_LOG_ERROR("Write malloc call back info size " << sizeof(AsyncCbInfo) << " failed");
        return OCK_RPC_ERR;
    }
    cbInfo->cb = cb->cb;
    cbInfo->arg = cb->arg;
    Channel_Callback callBack = {.cb = OckRpcCallbackFunc, .arg = static_cast<void *>(cbInfo)};
    int ret = 0;
    ret = Channel_Write(client, &reqMsg, &callBack);
    if (ret != OCK_RPC_OK) {
        free(cbInfo);
        return ret;
    }

    return OCK_RPC_OK;
}

int OckRpcWriteSgl(OckRpcClient client, OckRpcSglRequest *req, OckRpcCallDone *cb)
{
    if (cb == nullptr) {
        return Channel_WriteSgl(client, reinterpret_cast<Service_SglRequest *>(req), nullptr);
    }

    AsyncCbInfo *cbInfo = nullptr;
    cbInfo = reinterpret_cast<AsyncCbInfo *>(malloc(sizeof(AsyncCbInfo)));
    if (cbInfo == nullptr) {
        OCK_RPC_LOG_ERROR("WriteSgl malloc call back info size " << sizeof(AsyncCbInfo) << " failed");
        return OCK_RPC_ERR;
    }
    cbInfo->cb = cb->cb;
    cbInfo->arg = cb->arg;
    Channel_Callback callBack = {.cb = OckRpcCallbackFunc, .arg = static_cast<void *>(cbInfo)};
    int ret = 0;
    ret = Channel_WriteSgl(client, reinterpret_cast<Service_SglRequest *>(req), &callBack);
    if (ret != OCK_RPC_OK) {
        free(cbInfo);
        return ret;
    }

    return OCK_RPC_OK;
}