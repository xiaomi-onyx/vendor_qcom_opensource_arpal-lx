/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once

#include <PalDefs.h>
#include <log/log.h>
#include <utils/Thread.h>
#include <fmq/AidlMessageQueue.h>
#include <fmq/EventFlag.h>
#include <pal/PalAidlToLegacy.h>
#include <pal/PalLegacyToAidl.h>
#include <aidl/vendor/qti/hardware/pal/BnPALCallback.h>
#include <aidl/vendor/qti/hardware/pal/PalMessageQueueFlagBits.h>

using ::android::AidlMessageQueue;
using ::aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using ::android::hardware::EventFlag;
using ::android::Thread;
using android::sp;

namespace aidl::vendor::qti::hardware::pal {

class PalCallback : public BnPALCallback {
public:
    typedef AidlMessageQueue<int8_t, SynchronizedReadWrite> DataMQ;
    typedef AidlMessageQueue<PalReadWriteDoneCommand, SynchronizedReadWrite> CommandMQ;
    ::ndk::ScopedAStatus event_callback(int64_t in_streamHandle,
        int32_t in_event_id,
        int32_t in_event_data_size,
        const std::vector<uint8_t>& in_event_data,
        int64_t in_cookie) override;

    ::ndk::ScopedAStatus event_callback_rw_done(int64_t in_streamHandle,
        int32_t in_event_id,
        int32_t in_event_data_size,
        const std::vector<::aidl::vendor::qti::hardware::pal::PalCallbackBuffer>& in_rw_done_payload,
        int64_t in_cookie) override;
    ::ndk::ScopedAStatus prepare_mq_for_transfer(int64_t in_streamHandle,
        int64_t in_cookie,
        PalCallbackReturnData* _aidl_return) override;

    PalCallback(pal_stream_callback callBack)
    {
        if (callBack)
        {
            cb = callBack;
        }
    }

    virtual ~PalCallback();

protected:
    pal_stream_callback cb;
    std::unique_ptr<DataMQ> mDataMQ = nullptr;
    std::unique_ptr<CommandMQ> mCommandMQ = nullptr;
    EventFlag* mEfGroup = nullptr;
    std::atomic<bool> mStopDataTransferThread = false;
    sp<Thread> mDataTransferThread = nullptr;
    uint64_t mStreamCookie;
};

class DataTransferThread : public Thread {
   public:
    DataTransferThread(std::atomic<bool>* stop, int64_t streamHandle,
                pal_stream_callback clbkObject,
                PalCallback::DataMQ* dataMQ,
                PalCallback::CommandMQ* commandMQ,
                EventFlag* efGroup,
                uint64_t cookie)
        : Thread(false),
          mStop(stop),
          mStreamHandle(streamHandle),
          mStreamCallback(clbkObject),
          mDataMQ(dataMQ),
          mCommandMQ(commandMQ),
          mEfGroup(efGroup),
          mBuffer(nullptr),
          mStreamCookie(cookie) {}
    bool init() {
        mBuffer.reset(new (std::nothrow) int8_t[mDataMQ->getQuantumCount()]);
        return mBuffer != nullptr;
    }
    virtual ~DataTransferThread() {}

   private:
    std::atomic<bool>* mStop;
    uint64_t mStreamHandle;
    pal_stream_callback mStreamCallback;
    PalCallback::DataMQ* mDataMQ;
    PalCallback::CommandMQ* mCommandMQ;
    EventFlag* mEfGroup;
    std::unique_ptr<int8_t[]> mBuffer;
    uint64_t mStreamCookie;

    bool threadLoop() override;

    void startTransfer(int eventId);
};
}
