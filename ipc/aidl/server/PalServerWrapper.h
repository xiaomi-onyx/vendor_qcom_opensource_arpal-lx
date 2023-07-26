/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <aidl/vendor/qti/hardware/pal/BnPAL.h>
#include <aidl/vendor/qti/hardware/pal/IPALCallback.h>
#include <fmq/EventFlag.h>
#include <fmq/AidlMessageQueue.h>
#include <utils/Thread.h>
#include <mutex>
#include "PalApi.h"
#include <log/log.h>
#include <unordered_map>

using ::android::AidlMessageQueue;
using ::aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using ::android::hardware::EventFlag;
using ::android::Thread;

namespace aidl::vendor::qti::hardware::pal {

class SessionInfo {
    int64_t mHandle = 0;
    std::mutex mLock;

    using FdPair = std::pair<int, int>;
    std::vector<FdPair> mInOutFdPairs;
  public:
    SessionInfo(int64_t handle) : mHandle(handle) {
        ALOGI("SessionInfo created for handle %llx", mHandle);
    }
    ~SessionInfo();

    void addSharedMemoryFdPairs(int input, int dupFd);
    // remove the Fd and return input fd for this.
    int removeSharedMemoryFdPairs(int dupFd);
    void closeSharedMemoryFdPairs();
    void forceCloseSession();
};

class CallbackInfo {
    typedef ::android::AidlMessageQueue<
              int8_t, ::aidl::android::hardware::common::fmq::SynchronizedReadWrite> DataMQ;
    typedef ::android::AidlMessageQueue<
              PalReadWriteDoneCommand,
              ::aidl::android::hardware::common::fmq::SynchronizedReadWrite> CommandMQ;
  public:
    int mEventType;
    int64_t mClientData;
    int64_t mHandle;
    struct pal_stream_attributes session_attr;
    std::unique_ptr<DataMQ> mDataMQ = nullptr;
    std::unique_ptr<CommandMQ> mCommandMQ = nullptr;
    EventFlag* mEfGroup = nullptr;
    std::shared_ptr<IPALCallback> mCallback;
    CallbackInfo(const std::shared_ptr<IPALCallback> &callback, int64_t clientData) {
        mCallback = callback;
        mClientData = clientData;
        ALOGV("%s, callback %p handle %llx clientData %llu", __func__, callback.get(),
               clientData);
    }

    ~CallbackInfo() {
        ALOGV("%s, callback %p handle %llx clientData %llu", __func__, mCallback.get(),
               mHandle, mClientData);
        if (mEfGroup) {
            EventFlag::deleteEventFlag(&mEfGroup);
        }
    }
    void setSessionAttr(struct pal_stream_attributes *attr) {
        memcpy(&session_attr, attr, sizeof(session_attr));
    }
    void setHandle(int64_t handle) { mHandle = handle; }
    int32_t callReadWriteTransferThread(PalReadWriteDoneCommand cmd,
                            const int8_t* data, size_t dataSize);
    int32_t prepareMQForTransfer(int64_t streamHandle, int64_t cookie);
    int64_t getSessionId() { return mHandle; }
    int64_t getClientData() { return mClientData; }
    std::shared_ptr<IPALCallback> getCallback() { return mCallback; }
};

/*
* Interface for common session operations.
*/
class ISessionOps {
  public:
    virtual void addSessionHandle(int64_t handle) = 0;
    virtual void removeSessionHandle(int64_t handle) = 0;
    virtual void addSharedMemoryFdPairs(int64_t handle, int input, int dupFd) = 0;
    virtual int removeSharedMemoryFdPairs(int64_t handle, int dupFd) = 0;
    virtual void closeSharedMemoryFdPairs(int64_t handle) = 0;
    virtual ~ISessionOps() = default;
};

class PalServerWrapper;

class ClientInfo : public ISessionOps {
    std::vector<std::shared_ptr<CallbackInfo>> mCallbackInfo;
    // Handle vs sessionId relatedInfo

    int mPid = 0;
    std::mutex mCallbackLock;
    std::mutex mSessionLock;
    ::ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;
    static PalServerWrapper *sPalServerWrapper;

    public:
    std::unordered_map<int64_t, std::shared_ptr<SessionInfo>> mSessionsInfoMap;
    ClientInfo(int pid) : mPid(pid) {
        mDeathRecipient = ndk::ScopedAIBinder_DeathRecipient(
                AIBinder_DeathRecipient_new(ClientInfo::onDeath));
    }

    static void setPalServerWrapper(PalServerWrapper *wrapper);
    int getPid() { return mPid; }
    std::shared_ptr<SessionInfo> getSessionInfo_l(int64_t sessionId);
    // this could only happen when client goes out of scope
    virtual ~ClientInfo() { cleanup(); }
    void cleanup();
    // must be called with lock held.
    // keeps the SessionInfo object for given sessionId.
    // If it does not exist, creates a sessionInfo
    // Currently added with addSessionHandle and
    // removed with removeSessionHandle
    std::shared_ptr<SessionInfo> getSessionInfo_l(int32_t in_sessionId);

    void clearCallbacks();
    void clearSessions();
    void addSessionHandle(int64_t handle);
    void removeSessionHandle(int64_t handle);
    void addSharedMemoryFdPairs(int64_t handle, int input, int dupFd) override;
    // remove the Fd and return input fd for this.
    int removeSharedMemoryFdPairs(int64_t handle, int dupFd) override;
    void closeSharedMemoryFdPairs(int64_t handle) override;
    void registerCallback(int64_t handle, const std::shared_ptr<IPALCallback> &callback,
                                  std::shared_ptr<CallbackInfo> callBackInfo);
    void unregisterCallback(int64_t handle);
    static void onDeath(void *cookie);
    void onDeath();
    void getStreamMediaConfig(int64_t handle, pal_media_config *config);
    static int32_t pal_callback(pal_stream_handle_t *stream_handle,
                                uint32_t event_id, uint32_t *event_data,
                                uint32_t event_data_size,
                                uint64_t cookie);
};

class PalServerWrapper : public BnPAL, public ISessionOps {
    public:
    virtual ~PalServerWrapper() {}
    ::ndk::ScopedAStatus ipc_pal_stream_open(const PalStreamAttributes &attributes,
                                    const std::vector<PalDevice> &devices,
                                    const std::vector<ModifierKV> &modifiers,
                                    const std::shared_ptr<IPALCallback>& in_cb,
                                    int64_t ipc_clt_data,
                                    int64_t* _aidl_return) override;
    ::ndk::ScopedAStatus ipc_pal_stream_close(const int64_t streamHandle) override;
    ::ndk::ScopedAStatus ipc_pal_stream_start(const int64_t streamHandle) override;
    ::ndk::ScopedAStatus ipc_pal_stream_stop(const int64_t streamHandle) override;
    ::ndk::ScopedAStatus ipc_pal_stream_pause(const int64_t streamHandle) override;
    ::ndk::ScopedAStatus ipc_pal_stream_suspend(const int64_t streamHandle) override;
    ::ndk::ScopedAStatus ipc_pal_stream_resume(const int64_t streamHandle) override;
    ::ndk::ScopedAStatus ipc_pal_stream_flush(const int64_t streamHandle) override;
    ::ndk::ScopedAStatus ipc_pal_stream_drain(const int64_t streamHandle,
                                   const PalDrainType type) override;
    ::ndk::ScopedAStatus ipc_pal_stream_set_buffer_size(const int64_t streamHandle,
                                    const PalBufferConfig& rx_buff_cfg,
                                    const PalBufferConfig& tx_buff_cfg,
                                    std::vector<PalBufferConfig> * _aidl_return) override;
    ::ndk::ScopedAStatus ipc_pal_stream_get_buffer_size(const int64_t streamHandle,
                                    int32_t in_buf_size,
                                    int32_t out_buf_size,
                                    int32_t *_aidl_return) override;
    ::ndk::ScopedAStatus ipc_pal_stream_write(const int64_t streamHandle,
                                    const std::vector<PalBuffer> &buffer,
                                    int32_t *_aidl_return) override;
    ::ndk::ScopedAStatus ipc_pal_stream_read(const int64_t streamHandle,
                                     const std::vector<PalBuffer> &buffer,
                                     PalReadReturnData* _aidl_return) override;
    ::ndk::ScopedAStatus ipc_pal_stream_set_param(const int64_t streamHandle, int32_t param_id,
                                     const PalParamPayload &paramPayload) override;
    ::ndk::ScopedAStatus ipc_pal_stream_get_param(const int64_t streamHandle, int32_t param_id,
                                     PalParamPayload *paramPayload) override;
    ::ndk::ScopedAStatus ipc_pal_stream_get_device(const int64_t streamHandle,
                                                   std::vector<PalDevice> *devs) override;
    ::ndk::ScopedAStatus ipc_pal_stream_set_device(const int64_t streamHandle,
                                                   const std::vector<PalDevice> &devs) override;
    ::ndk::ScopedAStatus ipc_pal_stream_get_volume(const int64_t streamHandle,
                                                   PalVolumeData *_aidl_return) override;
    ::ndk::ScopedAStatus ipc_pal_stream_set_volume(const int64_t streamHandle,
                                                   const PalVolumeData &aidlVol) override;
    ::ndk::ScopedAStatus ipc_pal_stream_get_mute(const int64_t streamHandle,
                                                 bool *_aidl_return) override;
    ::ndk::ScopedAStatus ipc_pal_stream_set_mute(const int64_t streamHandle,
                                    bool state) override;
    ::ndk::ScopedAStatus ipc_pal_get_mic_mute(bool *_aidl_return) override;
    ::ndk::ScopedAStatus ipc_pal_set_mic_mute(bool state) override;
    ::ndk::ScopedAStatus ipc_pal_get_timestamp(const int64_t streamHandle,
                                               PalSessionTime* _aidl_return) override;
    ::ndk::ScopedAStatus ipc_pal_add_remove_effect(const int64_t streamHandle,
                                                   PalAudioEffect effect,
                                                   bool enable) override;
    ::ndk::ScopedAStatus ipc_pal_set_param(int32_t paramId,
                                      const std::vector<uint8_t> &payload) override;
    ::ndk::ScopedAStatus ipc_pal_get_param(int32_t paramId,
                                           std::vector<uint8_t> *_aidl_return) override;
    ::ndk::ScopedAStatus ipc_pal_stream_create_mmap_buffer(const int64_t streamHandle,
                              int32_t min_size_frames,
                              PalMmapBuffer* _aidl_return) override;
    ::ndk::ScopedAStatus ipc_pal_stream_get_mmap_position(const int64_t streamHandle,
                                                         PalMmapPosition* _aidl_return) override;
    ::ndk::ScopedAStatus ipc_pal_register_global_callback(const std::shared_ptr<IPALCallback>& cb, int64_t cookie) override;
    ::ndk::ScopedAStatus ipc_pal_gef_rw_param(int32_t paramId, const std::vector<uint8_t> &param_payload,
                                              PalDeviceId dev_id,
                                              PalStreamType stream_type, int8_t dir,
                                              std::vector<uint8_t> *_aidl_return) override;
    ::ndk::ScopedAStatus ipc_pal_stream_get_tags_with_module_info(const int64_t streamHandle,
                                                                  int32_t size,
                                                                  std::vector<uint8_t> *_aidl_return) override;
    void addSessionHandle(int64_t handle) override;
    void removeSessionHandle(int64_t handle) override;

    void addSharedMemoryFdPairs(int64_t handle, int input, int dupFd) override;
    // remove the Fd and return input fd for this.
    int removeSharedMemoryFdPairs(int64_t handle, int dupFd) override;
    void closeSharedMemoryFdPairs(int64_t handle) override;

    // it returns the client as per caller pid, must be called with lock held
    std::shared_ptr<ClientInfo> getClient_l();
    void removeClient(int pid);

    std::mutex mLock;
    // pid vs clientInfo
    std::unordered_map<int /*pid */, std::shared_ptr<ClientInfo>> mClients;
};
}
