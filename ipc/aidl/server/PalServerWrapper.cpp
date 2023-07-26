/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "PalServerWrapper.h"
#include "MetadataParser.h"
#include <pal/PalAidlToLegacy.h>
#include <pal/PalLegacyToAidl.h>
#include <pal/BinderStatus.h>
#include <aidl/vendor/qti/hardware/pal/PalMessageQueueFlagBits.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <pal/Utils.h>

#define MAX_CACHE_SIZE 64

using ndk::ScopedAStatus;

namespace aidl::vendor::qti::hardware::pal {
// map<FD, map<offset, input_frame_id>>
std::map<int, std::map<int32_t, int64_t>> gInputsPendingAck;
std::mutex gInputsPendingAckLock;

void addToPendingInputs(int fd, int32_t offset, int32_t ip_frame_id) {
    ALOGV("%s: fd %d, offset %u", __func__, fd, offset);
    std::lock_guard<std::mutex> pendingAcksLock(gInputsPendingAckLock);
    auto itFd = gInputsPendingAck.find(fd);
    if (itFd != gInputsPendingAck.end()) {
        gInputsPendingAck[fd][offset] = ip_frame_id;
        ALOGV("%s: added offset %u and frame id %u", __func__, offset,
               (int32_t)gInputsPendingAck[fd][offset]);
    } else {
        //create new map<offset,input_buffer_index> and add to FD map
        ALOGV("%s: added map for fd %lu", __func__, (unsigned long)fd);
        gInputsPendingAck.insert(std::make_pair(fd, std::map<int32_t, int64_t>()));
        ALOGV("%s: added frame id %lu for fd %d offset %u", __func__,
                    (unsigned long)ip_frame_id,  fd, (unsigned int)offset);
        gInputsPendingAck[fd].insert(std::make_pair(offset, ip_frame_id));
    }
}

int getInputBufferIndex(int fd, int32_t offset, int64_t &buf_index) {
    int status = 0;

    std::lock_guard<std::mutex> pendingAcksLock(gInputsPendingAckLock);
    ALOGV("%s: fd %d, offset %u", __func__, fd, offset);
    std::map<int, std::map<int32_t, int64_t>>::iterator itFd = gInputsPendingAck.find(fd);
    if (itFd != gInputsPendingAck.end()) {
        std::map<int32_t, int64_t> offsetToFrameIdxMap = itFd->second;
        auto itOffsetFrameIdxPair = offsetToFrameIdxMap.find(offset);
        if (itOffsetFrameIdxPair != offsetToFrameIdxMap.end()){
            buf_index = itOffsetFrameIdxPair->second;
            ALOGV("%s ip_frame_id=%lu", __func__, (unsigned long)buf_index);
        } else {
            status = -EINVAL;
            ALOGE("%s: Entry doesn't exist for FD 0x%x and offset 0x%x",
                    __func__, fd, offset);
        }
        gInputsPendingAck.erase(itFd);
    }
    return status;
}

SessionInfo::~SessionInfo() {
    ALOGV("%s handle %llx, fdPairs %d", __func__, mHandle, mInOutFdPairs.size());
}

void SessionInfo::forceCloseSession() {
    std::lock_guard<std::mutex> guard(mLock);
    if (mHandle) {
        ALOGV("force closing session with handle %llx", mHandle);
        pal_stream_stop((pal_stream_handle_t *)mHandle);
        pal_stream_close((pal_stream_handle_t *)mHandle);
    }
}

void SessionInfo::addSharedMemoryFdPairs(int inputFd, int dupFd) {
    std::lock_guard<std::mutex> guard(mLock);
    ALOGV("%s handle %llx Fds[input %d - dup %d] size %d", __func__, mHandle, inputFd, dupFd,
          mInOutFdPairs.size());
    mInOutFdPairs.push_back(std::make_pair(inputFd, dupFd));
}

int SessionInfo::removeSharedMemoryFdPairs(int dupFd) {
    std::lock_guard<std::mutex> guard(mLock);
    auto itr = mInOutFdPairs.begin();
    auto inputFd = -1;
    for (; itr != mInOutFdPairs.end(); itr++) {
        if (itr->second == dupFd) {
            inputFd = itr->first;
            itr = mInOutFdPairs.erase(itr);
            break;
        }
    }
    ALOGV("%s handle %llx Fds[input %d - dup %d] size %d", __func__, mHandle, inputFd, dupFd,
          mInOutFdPairs.size());
    return inputFd;
}

void SessionInfo::closeSharedMemoryFdPairs() {
    std::lock_guard<std::mutex> guard(mLock);
    ALOGI("Before %s handle %llx size %d", __func__, mHandle, mInOutFdPairs.size());
    auto itr = mInOutFdPairs.begin();
    for (; itr != mInOutFdPairs.end(); itr++) {
        close(itr->second);
    }
    mInOutFdPairs.clear();
    ALOGI("After %s handle %llx size %d", __func__, mHandle, mInOutFdPairs.size());
}

PalServerWrapper *ClientInfo::sPalServerWrapper = nullptr;
void ClientInfo::setPalServerWrapper(PalServerWrapper *wrapper) {
    sPalServerWrapper = wrapper;
}

void ClientInfo::addSharedMemoryFdPairs(int64_t handle, int inputFd, int dupFd)
{
    std::lock_guard<std::mutex> guard(mSessionLock);
    ALOGV("%s handle %llx, inputFd %d dupFd %d sessionSize %d", __func__, handle, inputFd, dupFd,
          mSessionsInfoMap.size());
    for (auto &sessionInfo : mSessionsInfoMap) {
        auto &sessionInfoObj = sessionInfo.second;
        if (sessionInfo.first == handle) {
            sessionInfoObj->addSharedMemoryFdPairs(inputFd, dupFd);
            break;
        }
    }
}

int ClientInfo::removeSharedMemoryFdPairs(int64_t handle, int dupFd) {
    std::lock_guard<std::mutex> guard(mSessionLock);

    auto inputFd = -1;
    for (auto &sessionInfo : mSessionsInfoMap) {
        auto &sessionInfoObj = sessionInfo.second;
        if (sessionInfo.first == handle) {
            inputFd = sessionInfoObj->removeSharedMemoryFdPairs(dupFd);
            break;
        }
    }
    ALOGV("%s handle %llx, inputFd %d dupFd %d sessionSize %d", __func__, handle, inputFd,
          dupFd, mSessionsInfoMap.size());
    return inputFd;
}

void ClientInfo::closeSharedMemoryFdPairs(int64_t handle) {
    std::lock_guard<std::mutex> guard(mSessionLock);
    for (auto &sessionInfo : mSessionsInfoMap) {
        auto &sessionInfoObj = sessionInfo.second;
        if (sessionInfo.first == handle) {
            sessionInfoObj->closeSharedMemoryFdPairs();
            break;
        }
    }
    ALOGV("%s handle %llx, sessionSize %d", __func__, handle,
          mSessionsInfoMap.size());
}

void ClientInfo::getStreamMediaConfig(int64_t handle, pal_media_config *config)
{
    std::lock_guard<std::mutex> guard(mSessionLock);
    ALOGV("%s handle %llx ", __func__, handle);
    for (auto &sessionInfo : mSessionsInfoMap) {
        auto &sessionInfoObj = sessionInfo.second;
        if (sessionInfo.first == handle) {
            auto itr = std::find_if(mCallbackInfo.begin(), mCallbackInfo.end(),
                            [=](const std::shared_ptr<CallbackInfo> &callback) {
                                return (callback->getSessionId() == handle);
                            });

            memcpy((int8_t *)config,
            (int8_t *)&itr->get()->session_attr.out_media_config,
            sizeof(pal_media_config));
            break;
        }
    }
}

void ClientInfo::registerCallback(int64_t handle, const std::shared_ptr<IPALCallback> &callback,
                                  const std::shared_ptr<CallbackInfo> callbackInfo) {
    ALOGV("%s, adding callback size %d ", __func__, mCallbackInfo.size());

    auto linkRet = AIBinder_linkToDeath(callback->asBinder().get(), mDeathRecipient.get(),
                                        this /* cookie */);
    if (linkRet != STATUS_OK) {
        ALOGV("%s, linkToDeath failed pid %d", __func__, mPid);
    } else {
        ALOGV("%s, linkToDeath success for client pid %d", __func__, mPid);
    }
    mCallbackInfo.emplace_back(callbackInfo);
}

void ClientInfo::unregisterCallback(int64_t handle) {
    // remove based on clientData from CallbackInfos.
    std::lock_guard<std::mutex> guard(mCallbackLock);
    ALOGV("%s, before removing callback size %d ", __func__, mCallbackInfo.size());
    auto itr = std::find_if(mCallbackInfo.begin(), mCallbackInfo.end(),
                            [=](const std::shared_ptr<CallbackInfo> &callback) {
                                return (callback->getSessionId() == handle);
                            });

    if (itr != mCallbackInfo.end()) {
        mCallbackInfo.erase(itr);
    }

    ALOGV("%s, after removing callback size %d ", __func__, mCallbackInfo.size());
}

void ClientInfo::onDeath(void *cookie) {
    ClientInfo *client = static_cast<ClientInfo *>(cookie);
    ALOGI("Client died (pid): %llu", client->getPid());
    client->onDeath();
}

void ClientInfo::onDeath() {
    sPalServerWrapper->removeClient(mPid);
}

void ClientInfo::clearSessions() {
    std::lock_guard<std::mutex> guard(mSessionLock);
    ALOGI("%s session size %d ", __func__, mSessionsInfoMap.size());
    for (const auto &session : mSessionsInfoMap) {
        session.second->forceCloseSession();
    }
    mSessionsInfoMap.clear();
}

void ClientInfo::clearCallbacks() {
    std::lock_guard<std::mutex> guard(mCallbackLock);
    ALOGV("client going out of scope clear callback of size %d", mCallbackInfo.size());
    mCallbackInfo.clear();
}

void ClientInfo::cleanup() {
    // Do a cleanup related to client going out of scope.
    ALOGI("%s client %d callbacks %d sessions %d ", __func__, mPid,
          mCallbackInfo.size(), mSessionsInfoMap.size());
    {
    std::lock_guard<std::mutex> guard(mSessionLock);
    for (const auto &session : mSessionsInfoMap) {
        session.second->closeSharedMemoryFdPairs();
    }
    }
    clearCallbacks();
    clearSessions();
}

std::shared_ptr<SessionInfo> ClientInfo::getSessionInfo_l(int64_t handle) {
    if (mSessionsInfoMap.count(handle) == 0) {
        ALOGV("new session %llx ", handle);
        mSessionsInfoMap[handle] = std::make_shared<SessionInfo>(handle);
    }
    return mSessionsInfoMap[handle];
}

void ClientInfo::addSessionHandle(int64_t handle) {
    std::lock_guard<std::mutex> guard(mSessionLock);

    auto sessionInfo = getSessionInfo_l(handle);
    ALOGI("%s handle %llx ", __func__, handle);
}

void ClientInfo::removeSessionHandle(int64_t handle) {
    std::lock_guard<std::mutex> guard(mSessionLock);
    ALOGV("%s,  removeSessionhandle %llx in session of size %d ", __func__, handle,
          mSessionsInfoMap.size());
    auto itr = mSessionsInfoMap.begin();
    for (; itr != mSessionsInfoMap.end();) {
        auto sessionInfo = itr->second;
        if (handle == itr->first) {
            ALOGI("%s removing handle %llx", __func__, handle);
            mSessionsInfoMap.erase(itr);
            break;
        }
        itr++;
    }
    ALOGV("%s, Exit: removeSessionhandle %llx in session of size %d ", __func__, handle,
          mSessionsInfoMap.size());
}

int32_t ClientInfo::pal_callback(pal_stream_handle_t *stream_handle,
                            uint32_t event_id, uint32_t *event_data,
                            uint32_t event_data_size,
                            uint64_t cookie)
{
    CallbackInfo* callbackInfo = (CallbackInfo *) cookie;
    IPALCallback* callbackBinder  = callbackInfo->mCallback.get();

    if (!AIBinder_isAlive(callbackBinder->asBinder().get())) {
        ALOGW("callback binder has died");
        return -EINVAL;
    }

    if ((callbackInfo->session_attr.type == PAL_STREAM_NON_TUNNEL) &&
          ((event_id == PAL_STREAM_CBK_EVENT_READ_DONE) ||
           (event_id == PAL_STREAM_CBK_EVENT_WRITE_READY))) {
        PalCallbackBuffer *rwDonePayload = NULL;
        std::vector<PalCallbackBuffer> rwDonePayloadAidl;
        struct pal_event_read_write_done_payload *rw_done_payload;
        int fdToBeClosed = -1;
        int inputFd = -1;

        rw_done_payload = (struct pal_event_read_write_done_payload *)event_data;
        /*
         * Find the original fd that was passed by client based on what
         * input and dup fd list and send that back.
         */
        int dupFd = rw_done_payload->buff.alloc_info.alloc_handle;
        inputFd = sPalServerWrapper->removeSharedMemoryFdPairs(int64_t(stream_handle), dupFd);
        if (inputFd != -1)
            fdToBeClosed = dupFd;
        rwDonePayloadAidl.resize(sizeof(pal_callback_buffer));
        rwDonePayload = (PalCallbackBuffer *)rwDonePayloadAidl.data();
        rwDonePayload->status = rw_done_payload->status;
        checkAndUpdateMDStatus(rw_done_payload, rwDonePayload);

        if (!rwDonePayload->status) {
            auto metadataParser = std::make_unique<MetadataParser>();
            if (event_id == PAL_STREAM_CBK_EVENT_READ_DONE) {
                auto cb_buf_info = std::make_unique<pal_clbk_buffer_info>();
                rwDonePayload->status = metadataParser->parseMetadata(
                            rw_done_payload->buff.metadata,
                            rw_done_payload->buff.metadata_size,
                            cb_buf_info.get());
                rwDonePayload->cbBufInfo.frame_index = cb_buf_info->frame_index;
                rwDonePayload->cbBufInfo.sample_rate = cb_buf_info->sample_rate;
                rwDonePayload->cbBufInfo.channel_count = cb_buf_info->channel_count;
                rwDonePayload->cbBufInfo.bit_width = cb_buf_info->bit_width;
            } else if (event_id == PAL_STREAM_CBK_EVENT_WRITE_READY) {
                rwDonePayload->status = getInputBufferIndex(
                            rw_done_payload->buff.alloc_info.alloc_handle,
                            rw_done_payload->buff.alloc_info.offset,
                            rwDonePayload->cbBufInfo.frame_index);
            }
            ALOGV("%s: frame_index=%u", __func__, rwDonePayload->cbBufInfo.frame_index);
        }

        rwDonePayload->size = rw_done_payload->buff.size;
        if (rw_done_payload->buff.ts != NULL) {
            rwDonePayload->timeStamp.tvSec =  rw_done_payload->buff.ts->tv_sec;
            rwDonePayload->timeStamp.tvNSec = rw_done_payload->buff.ts->tv_nsec;
        }
        if ((rw_done_payload->buff.buffer != NULL) &&
             !(callbackInfo->session_attr.flags & PAL_STREAM_FLAG_EXTERN_MEM)) {
            rwDonePayload->buffer.resize(rwDonePayload->size);
            memcpy(rwDonePayload->buffer.data(), rw_done_payload->buff.buffer,
                   rwDonePayload->size);
        }

        ALOGV("fd [input %d - dup %d]", inputFd, rw_done_payload->buff.alloc_info.alloc_handle);
        if (!callbackInfo->mDataMQ && !callbackInfo->mCommandMQ) {
            if (callbackInfo->prepareMQForTransfer(
                (int64_t)stream_handle, callbackInfo->mClientData)) {
                ALOGE("MQ prepare failed for stream %p", stream_handle);
            }
        }
        callbackInfo->callReadWriteTransferThread((PalReadWriteDoneCommand) event_id,
                                        (int8_t *)rwDonePayload,
                                        sizeof(PalCallbackBuffer));

        if (fdToBeClosed != -1) {
            ALOGV("closing dup fd %d ", fdToBeClosed);
            close(fdToBeClosed);
        } else {
            ALOGE("Error finding fd %d", rw_done_payload->buff.alloc_info.alloc_handle);
        }
    } else {
        std::vector<uint8_t> payload;
        payload.resize(event_data_size);
        memcpy(payload.data(), event_data, event_data_size);
        auto status = callbackBinder->event_callback((int64_t)stream_handle, event_id,
                              event_data_size, payload,
                              callbackInfo->mClientData);
        if (!status.isOk()) {
             ALOGE("%s: HIDL call failed during event_callback", __func__);
        }
    }
    return 0;
}

int32_t CallbackInfo::prepareMQForTransfer(int64_t streamHandle, int64_t cookie)
{
    std::unique_ptr<DataMQ> tempDataMQ;
    std::unique_ptr<CommandMQ> tempCommandMQ;
    PalReadWriteDoneResult retval;
    PalCallbackReturnData callbackData;

    auto ret = mCallback.get()->prepare_mq_for_transfer(streamHandle, cookie,
                                                    &callbackData);
    retval = callbackData.ret;
    if (retval == PalReadWriteDoneResult::OK) {
        tempDataMQ.reset(new DataMQ(callbackData.mqDataDesc));
        tempCommandMQ.reset(new CommandMQ(callbackData.mqCommandDesc));
        if (tempDataMQ->isValid() && tempDataMQ->getEventFlagWord()) {
            EventFlag::createEventFlag(tempDataMQ->getEventFlagWord(), &mEfGroup);
        }
    }
    if (retval != PalReadWriteDoneResult::OK) {
        return -ENODEV;
    }
    if (!tempDataMQ || !tempDataMQ->isValid() ||
            !tempCommandMQ || !tempCommandMQ->isValid() ||
            !mEfGroup) {
        ALOGE_IF(!tempDataMQ, "Failed to obtain data message queue for transfer");
        ALOGE_IF(tempDataMQ && !tempDataMQ->isValid(),
                 "Data message queue for transfer is invalid");
        ALOGE_IF(!tempCommandMQ, "Failed to obtain command message queue for transfer");
        ALOGE_IF(tempCommandMQ && !tempCommandMQ->isValid(),
                 "Command message queue for transfer is invalid");
        ALOGE_IF(!mEfGroup, "Event flag creation for transfer failed");
        return -ENODEV;
    }

    mDataMQ = std::move(tempDataMQ);
    mCommandMQ = std::move(tempCommandMQ);
    return 0;
}

int32_t CallbackInfo::callReadWriteTransferThread(PalReadWriteDoneCommand cmd,
                            const int8_t* data, size_t dataSize)
{
    if (!mCommandMQ->write(&cmd)) {
        ALOGE("command message queue write failed for %d", cmd);
        return -EAGAIN;
    }
    if (data != nullptr) {
        size_t availableToWrite = mDataMQ->availableToWrite();
        if (dataSize > availableToWrite) {
            ALOGW("truncating write data from %lld to %lld due to insufficient data queue space",
                    (long long)dataSize, (long long)availableToWrite);
            dataSize = availableToWrite;
        }
        if (!mDataMQ->write(data, dataSize)) {
            ALOGE("data message queue write failed");
        }
    }
    mEfGroup->wake(static_cast<int32_t>(PalMessageQueueFlagBits::NOT_EMPTY));

    uint32_t efState = 0;
retry:
    int32_t ret = mEfGroup->wait(static_cast<int32_t>(PalMessageQueueFlagBits::NOT_FULL), &efState);
    if (ret == -EAGAIN || ret == -EINTR) {
        // Spurious wakeup. This normally retries no more than once.
        ALOGE("%s: Wait returned error", __func__);
        goto retry;
    }
    return 0;
}

void PalServerWrapper::addSessionHandle(int64_t handle) {
    std::lock_guard<std::mutex> guard(mLock);
    ALOGV("%s, caller session %llx", __func__, handle);

    auto client = getClient_l();
    client->addSessionHandle(handle);
}

void PalServerWrapper::removeSessionHandle(int64_t handle) {
    std::lock_guard<std::mutex> guard(mLock);
    ALOGV("%s, caller handle %llx", __func__, handle);

    auto client = getClient_l();
    client->removeSessionHandle(handle);
}

void PalServerWrapper::addSharedMemoryFdPairs(int64_t handle, int inputFd, int dupFd)
{
    std::lock_guard<std::mutex> guard(mLock);
    ALOGV("%s, caller handle %llx inputFd %d dupFd %d", __func__, handle, inputFd, dupFd);

    auto client = getClient_l();
    client->addSharedMemoryFdPairs(handle, inputFd, dupFd);
}

int PalServerWrapper::removeSharedMemoryFdPairs(int64_t handle, int dupFd) {
    std::lock_guard<std::mutex> guard(mLock);
    int inputFd;
    int ret = -1 ;
    ALOGV("%s, caller handle %llx  dupFd %d", __func__, handle, dupFd);

    for (auto &client: mClients) {
        auto &clientInfoObj = client.second;
        inputFd = clientInfoObj->removeSharedMemoryFdPairs(handle, dupFd);
        if (inputFd != -1)
            ret = inputFd;
    }
    return ret;
}

void PalServerWrapper::closeSharedMemoryFdPairs(int64_t handle) {
    std::lock_guard<std::mutex> guard(mLock);
    ALOGV("%s, caller handle %llx", __func__, handle);

    auto client = getClient_l();
    client->closeSharedMemoryFdPairs(handle);
}

void PalServerWrapper::removeClient(int pid)
{
    std::lock_guard<std::mutex> guard(mLock);
    if (mClients.count(pid) != 0) {
        ALOGI("%s removing client %d", __func__, pid);
        mClients.erase(pid);
    } else {
        /*
        * client is already gone, nothing to do.
        */
    }
}

std::shared_ptr<ClientInfo> PalServerWrapper::getClient_l()
{
    int pid = AIBinder_getCallingPid();
    if (mClients.count(pid) == 0) {
        ALOGV("%s new client pid %d, total clients %d ", __func__, pid, mClients.size());
        mClients[pid] = std::make_shared<ClientInfo>(pid);
        ClientInfo::setPalServerWrapper(this);
    }
    return mClients[pid];
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_open(const PalStreamAttributes &attr,
                                                           const std::vector<PalDevice> &devs,
                                                           const std::vector<ModifierKV> &modskv,
                                                           const std::shared_ptr<IPALCallback>& cb,
                                                           int64_t ipc_clt_data,
                                                           int64_t* _aidl_return)
{
    pal_stream_handle_t *stream_handle = NULL;
    int64_t *handle;
    int cnt = 0;
    int32_t ret = -EINVAL;
    int8_t *temp = NULL;
    std::shared_ptr<CallbackInfo> callBackInfo;
    pal_stream_callback callback;
    auto client = getClient_l();
    PalDevUniquePtrType palDev(nullptr, free);
    PalModifierUniquePtrType modifiers(nullptr, free);

    if (cb) {
        callBackInfo = std::make_shared<CallbackInfo>(cb, ipc_clt_data);
        callback = ClientInfo::pal_callback;
    }

    auto palAttr = VALUE_OR_RETURN(allocate<pal_stream_attributes>(sizeof(struct pal_stream_attributes)));

    AidlToLegacy::convertPalStreamAttributes(attr, palAttr.get());

    if (devs.size()) {
        palDev = VALUE_OR_RETURN(allocate<pal_device>(sizeof(struct pal_device) * devs.size()));
        AidlToLegacy::convertPalDevice(devs, palDev.get());
    }
    if (modskv.size()) {
        modifiers = VALUE_OR_RETURN(allocate<modifier_kv>(sizeof(struct modifier_kv) * modskv.size()));
        AidlToLegacy::convertModifierKV(modskv, modifiers.get());
    }

    callBackInfo->setSessionAttr(palAttr.get());

    ret = pal_stream_open(palAttr.get(), devs.size(), palDev.get(), modskv.size(), modifiers.get(),
                          callback, (int64_t)callBackInfo.get(), &stream_handle);
    *_aidl_return = (int64_t) stream_handle;

    if (!ret) {
        addSessionHandle((int64_t) stream_handle);
        callBackInfo->setHandle((int64_t) stream_handle);
        client->registerCallback((int64_t) stream_handle, cb, callBackInfo);
    }
    return status_tToBinderResult(ret);
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_close(const int64_t streamHandle)
{
    int32_t ret = pal_stream_close((pal_stream_handle_t *)streamHandle);
    auto client = getClient_l();
    int pid = AIBinder_getCallingPid();

    client->unregisterCallback(streamHandle);
    closeSharedMemoryFdPairs(streamHandle);
    removeSessionHandle(streamHandle);
    if (client->mSessionsInfoMap.empty())
        removeClient(pid);
    return status_tToBinderResult(ret);
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_start(const int64_t streamHandle) {

    return status_tToBinderResult(pal_stream_start((pal_stream_handle_t *)streamHandle));
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_stop(const int64_t streamHandle) {
    return status_tToBinderResult(pal_stream_stop((pal_stream_handle_t *)streamHandle));
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_pause(const int64_t streamHandle) {
    return status_tToBinderResult(pal_stream_pause((pal_stream_handle_t *)streamHandle));
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_drain(const int64_t streamHandle, PalDrainType type)
{
    return status_tToBinderResult(pal_stream_drain((pal_stream_handle_t *)streamHandle,
                             (pal_drain_type_t) type));
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_flush(const int64_t streamHandle) {
    return status_tToBinderResult(pal_stream_flush((pal_stream_handle_t *)streamHandle));
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_suspend(const int64_t streamHandle) {
    return status_tToBinderResult(pal_stream_suspend((pal_stream_handle_t *)streamHandle));
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_resume(const int64_t streamHandle) {
    return status_tToBinderResult(pal_stream_resume((pal_stream_handle_t *)streamHandle));
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_set_buffer_size(const int64_t streamHandle,
                                             const PalBufferConfig& in_buff_config,
                                             const PalBufferConfig& out_buff_config,
                                             std::vector<PalBufferConfig> *_aidl_return)
{
    pal_buffer_config_t out_buf_cfg, in_buf_cfg;

    in_buf_cfg.buf_count = in_buff_config.buf_count;
    in_buf_cfg.buf_size = in_buff_config.buf_size;
    if (in_buff_config.max_metadata_size) {
        in_buf_cfg.max_metadata_size = in_buff_config.max_metadata_size;
    } else {
        in_buf_cfg.max_metadata_size = MetadataParser::WRITE_METADATA_MAX_SIZE();
    }

    out_buf_cfg.buf_count = out_buff_config.buf_count;
    out_buf_cfg.buf_size = out_buff_config.buf_size;
    if (out_buff_config.max_metadata_size) {
        out_buf_cfg.max_metadata_size = out_buff_config.max_metadata_size;
    } else {
        out_buf_cfg.max_metadata_size = MetadataParser::READ_METADATA_MAX_SIZE();
    }

    int32_t ret = pal_stream_set_buffer_size((pal_stream_handle_t *)streamHandle,
                                    &in_buf_cfg, &out_buf_cfg);

    auto in_buf_config = LegacyToAidl::convertPalBufferConfig(&in_buf_cfg);
    _aidl_return->push_back(in_buf_config);
    auto out_buf_config = LegacyToAidl::convertPalBufferConfig(&out_buf_cfg);
    _aidl_return->push_back(out_buf_config);

    return status_tToBinderResult(ret);
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_get_buffer_size(const int64_t streamHandle,
                                                 int32_t inBufSize,
                                                 int32_t outBufSize,
                                                 int32_t *_aidl_return)
{
    return ScopedAStatus::ok();
}


::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_write(const int64_t streamHandle,
                                                            const std::vector<PalBuffer> &inBuf,
                                                            int32_t *_aidl_return)
{
    struct pal_buffer buf = {0};

    buf.size = inBuf.data()->size;
    if (inBuf.data()->buffer.size() == buf.size) {
        buf.buffer = (uint8_t *)calloc(1, buf.size);
        if (!buf.buffer) {
            ALOGE("%s: failed to calloc", __func__);
            return status_tToBinderResult(-ENOMEM);
        }
    }
    buf.offset = (size_t)inBuf.data()->offset;
    auto timeStamp = std::make_unique<timespec>();
    timeStamp->tv_sec =  inBuf.data()->timeStamp.tvSec;
    timeStamp->tv_nsec = inBuf.data()->timeStamp.tvNSec;
    buf.ts = timeStamp.get();
    buf.flags = inBuf.data()->flags;
    buf.frame_index = inBuf.data()->frame_index;

    buf.metadata_size = MetadataParser::WRITE_METADATA_MAX_SIZE();
    if (buf.metadata_size) {
        buf.metadata = (uint8_t *)calloc(1, buf.metadata_size);
        if (!buf.metadata) {
            ALOGE("%s: failed to calloc", __func__);
            free(buf.buffer);
            return status_tToBinderResult(-ENOMEM);
        }
    }

    auto client = getClient_l();
    auto mediaConfig = VALUE_OR_RETURN(allocate<pal_media_config>(sizeof(struct pal_media_config)));
    client->getStreamMediaConfig(streamHandle, mediaConfig.get());

    auto metadataParser = std::make_unique<MetadataParser>();
    metadataParser->fillMetaData(buf.metadata, buf.frame_index, buf.size,
                                 mediaConfig.get());
    auto fdInfo = AidlToLegacy::getFdIntFromNativeHandle(inBuf.data()->alloc_info.alloc_handle);

    buf.alloc_info.alloc_handle = fdInfo.first;
    addSharedMemoryFdPairs(streamHandle, fdInfo.second, buf.alloc_info.alloc_handle);

    ALOGV("%s: fd[input%d - dup%d]", __func__, fdInfo.second, buf.alloc_info.alloc_handle);
    buf.alloc_info.alloc_size = inBuf.data()->alloc_info.alloc_size;
    buf.alloc_info.offset = inBuf.data()->alloc_info.offset;

    if (buf.buffer)
        memcpy(buf.buffer, inBuf.data()->buffer.data(), buf.size);
    ALOGV("%s:%d sz %d, frame_index %u", __func__,__LINE__, buf.size, buf.frame_index);

    addToPendingInputs(buf.alloc_info.alloc_handle,
                       buf.alloc_info.offset, buf.frame_index);

    int32_t ret = pal_stream_write((pal_stream_handle_t *)streamHandle, &buf);
    if (buf.metadata)
        free(buf.metadata);
    if (buf.buffer)
        free(buf.buffer);
    if (ret >= 0) {
        *_aidl_return = ret;
        return ScopedAStatus::ok();
    } else {
       return status_tToBinderResult(ret);
    }
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_read(const int64_t streamHandle,
                                                           const std::vector<PalBuffer> &inBuf,
                                                           PalReadReturnData* _aidl_return) {
    struct pal_buffer buf = {0};

    buf.size = inBuf.data()->size;
    buf.buffer = (uint8_t *)calloc(1, buf.size);
    if (!buf.buffer) {
        ALOGE("%s: failed to calloc", __func__);
        return status_tToBinderResult(-ENOMEM);
    }
    buf.metadata_size = MetadataParser::READ_METADATA_MAX_SIZE();
    auto fdHandle = AidlToLegacy::getFdIntFromNativeHandle(inBuf.data()->alloc_info.alloc_handle);

    buf.alloc_info.alloc_handle = (fdHandle.first);
    addSharedMemoryFdPairs(streamHandle, fdHandle.second, buf.alloc_info.alloc_handle);
    ALOGV("%s: fd[input%d - dup%d]", __func__, fdHandle.second, buf.alloc_info.alloc_handle);

    buf.alloc_info.alloc_size = inBuf.data()->alloc_info.alloc_size;
    buf.alloc_info.offset = inBuf.data()->alloc_info.offset;

    int32_t ret = pal_stream_read((pal_stream_handle_t *)streamHandle, &buf);
    _aidl_return->ret = ret;
    if (ret > 0) {
        _aidl_return->buffer.resize(sizeof(struct pal_buffer));
        _aidl_return->buffer.data()->size = (uint32_t)buf.size;
        _aidl_return->buffer.data()->offset = (uint32_t)buf.offset;
        _aidl_return->buffer.data()->buffer.resize(buf.size);
        memcpy(_aidl_return->buffer.data()->buffer.data(), buf.buffer,
                buf.size);
        if (buf.ts) {
           _aidl_return->buffer.data()->timeStamp.tvSec = buf.ts->tv_sec;
           _aidl_return->buffer.data()->timeStamp.tvNSec = buf.ts->tv_nsec;
        }
        ALOGV("%s ret %d size %d", __func__, ret, _aidl_return->buffer.data()->size);
        return ::ndk::ScopedAStatus::ok();
    }
    return status_tToBinderResult(ret);
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_set_param(const int64_t streamHandle, int32_t paramId,
                                        const PalParamPayload &paramPayload)
{
    int32_t payloadSize = paramPayload.payload.size();
    auto palParamPayload = VALUE_OR_RETURN(allocate<pal_param_payload>(sizeof(pal_param_payload) +
                                                                       paramPayload.payload.size()));
    palParamPayload->payload_size = paramPayload.payload.size();
    memcpy(palParamPayload.get()->payload, paramPayload.payload.data(), payloadSize);
    return status_tToBinderResult(pal_stream_set_param((pal_stream_handle_t *)streamHandle, paramId,
                                                 palParamPayload.get()));
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_get_param(const int64_t streamHandle,
                                         int32_t paramId,
                                         PalParamPayload* _aidl_return)
{
    pal_param_payload *param_payload;
    int32_t ret = pal_stream_get_param((pal_stream_handle_t *)streamHandle, paramId, &param_payload);
//    if (!ret) {
       //TODO see about memory allocation
//        memcpy(_aidl_return->payload, param_payload->payload,
//               param_payload->payload_size);
//    }
    return status_tToBinderResult(ret);
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_get_device(const int64_t streamHandle,
                                                                 std::vector<PalDevice> *devs)
{
    return ScopedAStatus::ok();
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_set_device(const int64_t streamHandle,
                                                                 const std::vector<PalDevice> &devs)
{
    int32_t devSize = devs.size();
    auto palDevices = VALUE_OR_RETURN(allocate<pal_device>(devSize * sizeof(struct pal_device)));

    AidlToLegacy::convertPalDevice(devs, palDevices.get());
    return status_tToBinderResult(pal_stream_set_device((pal_stream_handle_t *)streamHandle,
                                  devSize, palDevices.get()));
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_get_volume(const int64_t streamHandle,
                                                                 PalVolumeData *_aidl_return)
{
    return ScopedAStatus::ok();
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_set_volume(const int64_t streamHandle,
                                    const PalVolumeData &aidlVol)
{
    int32_t noOfVolPairs = aidlVol.volPair.size();
    int32_t volSize = sizeof(struct pal_volume_data) + noOfVolPairs * sizeof(pal_channel_vol_kv);
    auto palVolume = VALUE_OR_RETURN(allocate<pal_volume_data>(volSize));

    AidlToLegacy::convertPalVolumeData(aidlVol, palVolume.get());
    return status_tToBinderResult(pal_stream_set_volume((pal_stream_handle_t *)streamHandle, palVolume.get()));
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_get_mute(const int64_t streamHandle,
                                                               bool *_aidl_return)
{
    return ScopedAStatus::ok();
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_set_mute(const int64_t streamHandle,
                                    bool state)
{
    return status_tToBinderResult(pal_stream_set_mute((pal_stream_handle_t *)streamHandle, state));
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_get_mic_mute(bool *_aidl_return)
{
    return ScopedAStatus::ok();
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_set_mic_mute(bool state)
{
    return ScopedAStatus::ok();
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_get_timestamp(const int64_t streamHandle,
                                                             PalSessionTime *_aidl_return)
{
    struct pal_session_time stime;
    int32_t ret = pal_get_timestamp((pal_stream_handle_t *)streamHandle, &stime);
    if (!ret) {
       LegacyToAidl::convertPalSessionTimeToAidl(&stime, _aidl_return);
    }
    return status_tToBinderResult(ret);
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_add_remove_effect(const int64_t streamHandle,
                                          PalAudioEffect effect,
                                          bool enable)
{
    return status_tToBinderResult(pal_add_remove_effect((pal_stream_handle_t *)streamHandle,
                                   (pal_audio_effect_t) effect, enable));
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_set_param(int32_t paramId,
                                       const std::vector<uint8_t> &paramPayload)
{
    uint32_t paramSize = paramPayload.size() * sizeof(uint8_t);
    auto palParamPayload = VALUE_OR_RETURN(allocate<int8_t>(paramSize));
    memcpy(palParamPayload.get(), paramPayload.data(), paramSize);

    return status_tToBinderResult(pal_set_param(paramId, (void *)palParamPayload.get(), paramSize));
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_get_param(int32_t paramId,
                                                         std::vector<uint8_t> *_aidl_return)
{
    void *payload;
    size_t payloadSize;
    int32_t ret = pal_get_param(paramId, &payload, &payloadSize, NULL);
    if (!ret && payloadSize > 0) {
        _aidl_return->resize(payloadSize);
        memcpy(_aidl_return->data(), payload, payloadSize);
    }
    return status_tToBinderResult(ret);
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_create_mmap_buffer(const int64_t streamHandle,
                                                                         int32_t min_size_frames,
                                                                         PalMmapBuffer* _aidl_return)
{
    struct pal_mmap_buffer info;
    int32_t ret = pal_stream_create_mmap_buffer((pal_stream_handle_t *)streamHandle, min_size_frames, &info);
    if (!ret) {
       LegacyToAidl::convertMmapBufferInfoToAidl(&info, _aidl_return);
    }
    return status_tToBinderResult(ret);
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_get_mmap_position(const int64_t streamHandle,
                                                                        PalMmapPosition* _aidl_return)
{
    struct pal_mmap_position mmap_position;
    int32_t ret = pal_stream_get_mmap_position((pal_stream_handle_t *)streamHandle, &mmap_position);
    if (!ret) {
        LegacyToAidl::convertMmapPositionInfoToAidl(&mmap_position, _aidl_return);
    }
    return status_tToBinderResult(ret);
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_register_global_callback(const std::shared_ptr<IPALCallback>& cb,
                                                                        int64_t cookie)
{
    return ScopedAStatus::ok();
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_gef_rw_param(int32_t paramId, const std::vector<uint8_t> &param_payload,
                                                            PalDeviceId dev_id,
                                                            PalStreamType stream_type, int8_t dir,
                                                            std::vector<uint8_t> *_aidl_return)
{
    return ScopedAStatus::ok();
}

::ndk::ScopedAStatus PalServerWrapper::ipc_pal_stream_get_tags_with_module_info(const int64_t streamHandle,
                                                                                int32_t size,
                                                                                std::vector<uint8_t> *_aidl_return)
{
    uint8_t *palPayload = NULL;
    size_t payloadSize = size;

    if (size > 0) {
        palPayload = (uint8_t *)calloc(1, payloadSize);
        if (palPayload == NULL) {
            ALOGE("%s: Cannot allocate memory for pal payload ", __func__);
            return status_tToBinderResult(-ENOMEM);
        }
    }
    int32_t ret = pal_stream_get_tags_with_module_info((pal_stream_handle_t *)streamHandle, &payloadSize, palPayload);

    if (!ret && (payloadSize <= size) && palPayload != NULL) {
        _aidl_return->resize(payloadSize);
        memcpy(_aidl_return->data(), palPayload, payloadSize);
    }
    free(palPayload);
    return status_tToBinderResult(ret);
}
}
