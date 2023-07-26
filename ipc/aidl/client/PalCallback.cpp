/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */
#include <aidl/vendor/qti/hardware/pal/IPAL.h>
#include <aidl/vendor/qti/hardware/pal/BnPALCallback.h>
#include <aidl/vendor/qti/hardware/pal/PalReadWriteDoneResult.h>
#include "PalCallback.h"
#include <log/log.h>
#include <pal/BinderStatus.h>

using android::status_t;
using android::sp;

namespace aidl::vendor::qti::hardware::pal {

void DataTransferThread::startTransfer(int eventId) {
    size_t availToRead = mDataMQ->availableToRead();

    if (mDataMQ->read(&mBuffer[0], availToRead)) {
        ALOGV("%s: calling client callback, data size %zu", __func__, availToRead);

        const PalCallbackBuffer *rwDonePayload = (PalCallbackBuffer *)&mBuffer[0];
        auto cbBuffer = std::make_unique<pal_callback_buffer>();
        AidlToLegacy::convertPalCallbackBuffer(rwDonePayload, cbBuffer.get());
        mStreamCallback((pal_stream_handle_t *)mStreamHandle, eventId,
                        (uint32_t *)cbBuffer.get(), (uint32_t)availToRead,
                        mStreamCookie);
    }
}

bool DataTransferThread::threadLoop() {
    while (!std::atomic_load_explicit(mStop, std::memory_order_acquire)) {
        uint32_t efState = 0;
        mEfGroup->wait(static_cast<uint32_t>(PalMessageQueueFlagBits::NOT_EMPTY), &efState);
        if (!(efState & static_cast<uint32_t>(PalMessageQueueFlagBits::NOT_EMPTY))) {
            continue;  // Nothing to do.
        }
        PalReadWriteDoneCommand eventId;
        if (!mCommandMQ->read(&eventId)) {
            continue;
        }
        startTransfer((int)eventId);
        mEfGroup->wake(static_cast<uint32_t>(PalMessageQueueFlagBits::NOT_FULL));
    }

    return false;
}

::ndk::ScopedAStatus PalCallback::prepare_mq_for_transfer(int64_t streamHandle,
                            int64_t cookie, PalCallbackReturnData* callbackData) {
    status_t status;
    // Create message queues.
    if (mDataMQ) {
        ALOGE("the client attempts to call prepareForWriting twice");
        callbackData->ret = PalReadWriteDoneResult::INVALID_STATE;
        //TODO See if better retrun value can be returned.
        return status_tToBinderResult(-EINVAL);
    }

    std::unique_ptr<DataMQ> tempDataMQ(
            new DataMQ(sizeof(PalCallbackBuffer) * 2, true /* EventFlag */));

    std::unique_ptr<CommandMQ> tempCommandMQ(new CommandMQ(1));
    if (!tempDataMQ->isValid() || !tempCommandMQ->isValid()) {
        ALOGE_IF(!tempDataMQ->isValid(), "data MQ is invalid");
        ALOGE_IF(!tempCommandMQ->isValid(), "command MQ is invalid");
        callbackData->ret = PalReadWriteDoneResult::INVALID_ARGUMENTS;
        return status_tToBinderResult(-EINVAL);
    }
    EventFlag* tempRawEfGroup{};
    status = EventFlag::createEventFlag(tempDataMQ->getEventFlagWord(), &tempRawEfGroup);
    std::unique_ptr<EventFlag, void (*)(EventFlag*)> tempElfGroup(
        tempRawEfGroup, [](auto* ef) { EventFlag::deleteEventFlag(&ef); });
    if (status != ::android::OK || !tempElfGroup) {
        ALOGE("failed creating event flag for data MQ: %s", strerror(-status));
        callbackData->ret = PalReadWriteDoneResult::INVALID_ARGUMENTS;
        return status_tToBinderResult(-EINVAL);
    }

    // Create and launch the thread.
    auto tempDataTransferThread = sp<DataTransferThread>::make(
                                  &mStopDataTransferThread, streamHandle, cb, tempDataMQ.get(),
                                  tempCommandMQ.get(), tempElfGroup.get(), cookie);
    if (!tempDataTransferThread->init()) {
        ALOGW("failed to start writer thread: %s", strerror(-status));
        callbackData->ret = PalReadWriteDoneResult::INVALID_ARGUMENTS;
        return status_tToBinderResult(-EINVAL);
    }
    status = tempDataTransferThread->run("read_write_cb", ::android::PRIORITY_URGENT_AUDIO);
    if (status != ::android::OK) {
        ALOGW("failed to start read_write_cb thread: %s", strerror(-status));
        callbackData->ret = PalReadWriteDoneResult::INVALID_ARGUMENTS;
        return status_tToBinderResult(-EINVAL);
    }

    mDataMQ = std::move(tempDataMQ);
    mCommandMQ = std::move(tempCommandMQ);
    mDataTransferThread = tempDataTransferThread;
    mEfGroup = tempElfGroup.release();
    callbackData->ret = PalReadWriteDoneResult::OK;
    callbackData->mqDataDesc =  mDataMQ->dupeDesc();
    callbackData->mqCommandDesc =  mCommandMQ->dupeDesc();
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus PalCallback::event_callback(int64_t in_streamHandle,
                                                 int32_t in_event_id,
                                                 int32_t in_event_data_size,
                                                 const std::vector<uint8_t>& in_event_data,
                                                 int64_t in_cookie)
{
    uint32_t *ev_data = NULL;
    int8_t *src = NULL;
    ev_data = (uint32_t *) calloc(1, in_event_data_size);
    if (!ev_data) {
        goto exit;
    }

    src = (int8_t *) in_event_data.data();
    memcpy(ev_data, src, in_event_data_size);

    this->cb((pal_stream_handle_t *) in_streamHandle, in_event_id, ev_data, in_event_data_size, in_cookie);

    exit:
        if (ev_data){
            free(ev_data);
        }
        return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus PalCallback::event_callback_rw_done(int64_t in_streamHandle,
        int32_t in_event_id,
	int32_t in_event_data_size,
        const std::vector<::aidl::vendor::qti::hardware::pal::PalCallbackBuffer>& in_rw_done_payload,
        int64_t in_cookie)
{
    const ::aidl::vendor::qti::hardware::pal::PalCallbackBuffer *rwDonePayload = in_rw_done_payload.data();
    auto cbBuffer = std::make_unique<pal_callback_buffer>();

    AidlToLegacy::convertPalCallbackBuffer(rwDonePayload, cbBuffer.get());
    this->cb((pal_stream_handle_t *) in_streamHandle, in_event_id, (uint32_t *) cbBuffer.get(), in_event_data_size, in_cookie);

    return ::ndk::ScopedAStatus::ok();
}

PalCallback::~PalCallback()
{
    mStopDataTransferThread.store(true, std::memory_order_release);
    if (mEfGroup) {
        mEfGroup->wake(static_cast<uint32_t>(PalMessageQueueFlagBits::NOT_EMPTY));
    }
    if (mDataTransferThread.get()) {
        status_t status = mDataTransferThread->join();
        ALOGE_IF(status, "write thread exit error: %s", strerror(-status));
    }
    if (mEfGroup) {
        status_t status = EventFlag::deleteEventFlag(&mEfGroup);
        ALOGE_IF(status, "write MQ event flag deletion error: %s", strerror(-status));
    }
}
}
