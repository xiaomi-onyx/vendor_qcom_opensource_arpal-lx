/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PalIpc::AidlToLegacy::Converter"

#include <pal/PalAidlToLegacy.h>
#include <pal/Utils.h>
#include <log/log.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <PalDefs.h>

namespace aidl::vendor::qti::hardware::pal {

void AidlToLegacy::convertPalStreamAttributes(const PalStreamAttributes &aidlConfig,
                                struct pal_stream_attributes *palStreamAttributes)
{
    palStreamAttributes->type = (pal_stream_type_t)aidlConfig.type;
    palStreamAttributes->info.opt_stream_info.version = aidlConfig.info.version;
    palStreamAttributes->info.opt_stream_info.size = aidlConfig.info.size;
    palStreamAttributes->info.opt_stream_info.duration_us = aidlConfig.info.duration_us;
    palStreamAttributes->info.opt_stream_info.has_video = aidlConfig.info.has_video;
    palStreamAttributes->info.opt_stream_info.is_streaming = aidlConfig.info.is_streaming;
    palStreamAttributes->flags = (pal_stream_flags_t)aidlConfig.flags;
    palStreamAttributes->direction = (pal_stream_direction_t)aidlConfig.direction;
    palStreamAttributes->in_media_config.sample_rate =
                     aidlConfig.in_media_config.sample_rate;
    palStreamAttributes->in_media_config.bit_width = aidlConfig.in_media_config.bit_width;
    palStreamAttributes->in_media_config.aud_fmt_id =
                     (pal_audio_fmt_t)aidlConfig.in_media_config.aud_fmt_id;
    palStreamAttributes->in_media_config.ch_info.channels = aidlConfig.in_media_config.ch_info.channels;
    memcpy(&palStreamAttributes->in_media_config.ch_info.ch_map, &aidlConfig.in_media_config.ch_info.ch_map,
           sizeof(uint8_t [64]));
    palStreamAttributes->out_media_config.sample_rate = aidlConfig.out_media_config.sample_rate;
    palStreamAttributes->out_media_config.bit_width =
                        aidlConfig.out_media_config.bit_width;
    palStreamAttributes->out_media_config.aud_fmt_id =
                        (pal_audio_fmt_t)aidlConfig.out_media_config.aud_fmt_id;
    palStreamAttributes->out_media_config.ch_info.channels = aidlConfig.out_media_config.ch_info.channels;
    memcpy(&palStreamAttributes->out_media_config.ch_info.ch_map, &aidlConfig.out_media_config.ch_info.ch_map,
           sizeof(uint8_t [64]));
}

void AidlToLegacy::convertPalDevice(const std::vector<PalDevice> &aidlDevConfig, struct pal_device *palDevice)
{
    for (unsigned long i = 0; i < aidlDevConfig.size(); i++) {
        palDevice[i].id = (pal_device_id_t)aidlDevConfig[i].id;
        palDevice[i].config.sample_rate = aidlDevConfig[i].config.sample_rate;
        palDevice[i].config.bit_width = aidlDevConfig[i].config.bit_width;
        palDevice[i].config.ch_info.channels = aidlDevConfig[i].config.ch_info.channels;
        memcpy(&palDevice[i].config.ch_info.ch_map, &aidlDevConfig[i].config.ch_info.ch_map,
                   sizeof(uint8_t [64]));
        palDevice[i].config.aud_fmt_id =
                                (pal_audio_fmt_t)aidlDevConfig[i].config.aud_fmt_id;
    }
}

void AidlToLegacy::convertModifierKV(const std::vector<ModifierKV> &aidlKv, struct modifier_kv *modifierKv)
{
    for (unsigned long i = 0; i < aidlKv.size(); i++) {
        modifierKv[i].key = aidlKv[i].key;
        modifierKv[i].value =  aidlKv[i].value;
    }
}

void AidlToLegacy::convertPalBufferConfig(const PalBufferConfig &aidlConfig,
                            struct pal_buffer_config *palBufferConfig)
{
}

void AidlToLegacy::convertPalBuffer(const PalBuffer &aidlConfig, struct pal_buffer *palBuffer,
                            bool externalMemory, bool copyBuffers)
{
}

void AidlToLegacy::convertPalParamPayload(const PalParamPayload &aidlConfig,
                            pal_param_payload *palParamPayload)
{
}

void AidlToLegacy::convertPalVolumeData(const PalVolumeData &aidlVolConfig, pal_volume_data *palVolumeData)
{
    palVolumeData->no_of_volpair = aidlVolConfig.volPair.size();
    for (unsigned long i = 0; i < aidlVolConfig.volPair.size(); i++) {
        palVolumeData->volume_pair[i].channel_mask = aidlVolConfig.volPair[i].chMask;
        palVolumeData->volume_pair[i].vol = aidlVolConfig.volPair[i].vol;
    }
}

std::pair<int, int> AidlToLegacy::getFdIntFromNativeHandle(
        const aidl::android::hardware::common::NativeHandle &nativeHandle, bool doDup)
{
    std::pair<int, int> fdIntPair = {-1, -1};
    if (!nativeHandle.fds.empty()) {
        if (doDup) {
            fdIntPair.first = dup(nativeHandle.fds.at(0).get());
        } else {
            fdIntPair.first = (nativeHandle.fds.at(0).get());
        }
    }
    if (!nativeHandle.ints.empty()) {
        fdIntPair.second = nativeHandle.ints.at(0);
    }

    return std::move(fdIntPair);
}

void AidlToLegacy::convertPalCallbackBuffer(const PalCallbackBuffer* rwDonePayload,
                                            pal_callback_buffer* cbBuffer)
{
    cbBuffer->size = rwDonePayload->size;
    std::vector<uint8_t> buffData = {};
    if (rwDonePayload->buffer.size() == cbBuffer->size) {
        buffData.resize(cbBuffer->size);
        memcpy(buffData.data(), rwDonePayload->buffer.data(), cbBuffer->size);
        cbBuffer->buffer = buffData.data();
    }

    auto bufTimeSpec = std::make_unique<timespec>();
    if (!bufTimeSpec) {
        ALOGE("%s: Failed to allocate memory for timespec", __func__);
        return;
    }
    bufTimeSpec->tv_sec = rwDonePayload->timeStamp.tvSec;
    bufTimeSpec->tv_nsec = rwDonePayload->timeStamp.tvNSec;
    cbBuffer->ts = (timespec *) bufTimeSpec.get();
    cbBuffer->status = rwDonePayload->status;
    cbBuffer->cb_buf_info.frame_index = rwDonePayload->cbBufInfo.frame_index;
    cbBuffer->cb_buf_info.sample_rate = rwDonePayload->cbBufInfo.sample_rate;
    cbBuffer->cb_buf_info.bit_width = rwDonePayload->cbBufInfo.bit_width;
    cbBuffer->cb_buf_info.channel_count = rwDonePayload->cbBufInfo.channel_count;
}

void AidlToLegacy::convertPalSessionTime(const PalSessionTime &aildSessTime, struct pal_session_time* palSessTime)
{
    palSessTime->session_time.value_lsw = aildSessTime.session_time.valLsw;
    palSessTime->session_time.value_msw = aildSessTime.session_time.valMsw;
    palSessTime->absolute_time.value_lsw = aildSessTime.absolute_time.valLsw;
    palSessTime->absolute_time.value_msw = aildSessTime.absolute_time.valMsw;
    palSessTime->timestamp.value_lsw = aildSessTime.timestamp.valLsw;
    palSessTime->timestamp.value_msw = aildSessTime.timestamp.valMsw;
}
}
