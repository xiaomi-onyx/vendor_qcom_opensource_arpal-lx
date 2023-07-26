/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#define LOG_TAG "PalIpc::LegacyToAidl::Converter"

#include <pal/PalLegacyToAidl.h>
#include <pal/Utils.h>
#include <PalDefs.h>
#include <log/log.h>
#include <aidlcommonsupport/NativeHandle.h>
#include <aidl/vendor/qti/hardware/pal/PalStreamAttributes.h>
#include <aidl/vendor/qti/hardware/pal/PalDevice.h>
#include <aidl/vendor/qti/hardware/pal/ModifierKV.h>
#include <aidl/vendor/qti/hardware/pal/PalDrainType.h>
#include <aidl/vendor/qti/hardware/pal/PalBufferConfig.h>
#include <aidl/vendor/qti/hardware/pal/PalBuffer.h>
#include <aidl/vendor/qti/hardware/pal/PalParamPayload.h>
#include <aidl/vendor/qti/hardware/pal/PalMmapPosition.h>
#include <aidl/vendor/qti/hardware/pal/PalMmapBuffer.h>
#include <aidl/vendor/qti/hardware/pal/PalAudioEffect.h>

namespace aidl::vendor::qti::hardware::pal {

PalStreamAttributes LegacyToAidl::convertPalStreamAttributes(struct pal_stream_attributes *palStreamAttr)
{
    PalStreamAttributes aidlStreamAttr;
    PalStreamInfo aidlStreamInfo;
    pal_stream_info palStreamInfo;
    PalMediaConfig aidlInMediaConfig;
    PalMediaConfig aidlOutMediaConfig;
    PalChannelInfo aidlInChannelInfo;
    PalChannelInfo aidlOutChannelInfo;

    if (palStreamAttr == nullptr) {
        return {};
    }

    aidlStreamAttr.type = static_cast<PalStreamType>(palStreamAttr->type);
    uint16_t in_channels = palStreamAttr->in_media_config.ch_info.channels;
    uint16_t out_channels = palStreamAttr->out_media_config.ch_info.channels;

    aidlStreamAttr.type = static_cast<PalStreamType>(palStreamAttr->type);

    ALOGD("%s: %d channels[in %d : out %d] format[in %d : out %d] flags %d",__func__,__LINE__,
        in_channels, out_channels, palStreamAttr->in_media_config.aud_fmt_id,
        palStreamAttr->out_media_config.aud_fmt_id, palStreamAttr->flags);
    // AIDL Stream Info
    palStreamInfo = palStreamAttr->info.opt_stream_info;
    aidlStreamInfo.version = static_cast<long>(palStreamInfo.version);
    aidlStreamInfo.size = static_cast<long>(palStreamInfo.size);
    aidlStreamInfo.duration_us = static_cast<long>(palStreamInfo.duration_us);
    aidlStreamInfo.has_video = palStreamInfo.has_video;
    aidlStreamInfo.is_streaming = palStreamInfo.is_streaming;
    aidlStreamInfo.loopback_type = palStreamInfo.loopback_type;
    aidlStreamInfo.haptics_type = palStreamInfo.haptics_type;
    aidlStreamAttr.info = aidlStreamInfo;

    aidlStreamAttr.flags = static_cast<PalStreamFlag>(palStreamAttr->flags);

    aidlStreamAttr.direction = static_cast<PalStreamDirection>(palStreamAttr->direction);

    // InMediaConfig
    aidlInMediaConfig.sample_rate = static_cast<int>(palStreamAttr->in_media_config.sample_rate);
    aidlInMediaConfig.bit_width = static_cast<int>(palStreamAttr->in_media_config.bit_width);
    aidlInChannelInfo.channels = palStreamAttr->in_media_config.ch_info.channels;
    memcpy(aidlInChannelInfo.ch_map.data(), palStreamAttr->in_media_config.ch_info.ch_map, PAL_MAX_CHANNELS_SUPPORTED);
    aidlInMediaConfig.aud_fmt_id = static_cast<PalAudioFmt>(palStreamAttr->in_media_config.aud_fmt_id);
    aidlInMediaConfig.ch_info = aidlInChannelInfo;
    aidlStreamAttr.in_media_config = aidlInMediaConfig;

    //OutMediaConfig
    aidlOutMediaConfig.sample_rate = static_cast<int>(palStreamAttr->out_media_config.sample_rate);
    aidlOutMediaConfig.bit_width = static_cast<int>(palStreamAttr->out_media_config.bit_width);
    aidlOutChannelInfo.channels = palStreamAttr->out_media_config.ch_info.channels;
    memcpy(aidlOutChannelInfo.ch_map.data(), palStreamAttr->out_media_config.ch_info.ch_map, PAL_MAX_CHANNELS_SUPPORTED);
    aidlOutMediaConfig.aud_fmt_id = static_cast<PalAudioFmt>(palStreamAttr->out_media_config.aud_fmt_id);
    aidlOutMediaConfig.ch_info = aidlOutChannelInfo;
    aidlStreamAttr.out_media_config = aidlOutMediaConfig;

    ALOGV("%s config %s", __func__, aidlStreamAttr.toString().c_str());
    return std::move(aidlStreamAttr);
}

PalDevice LegacyToAidl::convertPalDevice(struct pal_device *palDevice)
{
    PalDevice aidlPalDevice;
    PalMediaConfig aidlMediaConfig;
    PalChannelInfo aidlChannelInfo;
    PalUsbDeviceAddress aidlAddress;

    if (palDevice == nullptr) {
        return {};
    }

    aidlPalDevice.id = static_cast<PalDeviceId>(palDevice->id);

    // AIDL Media Config
    aidlMediaConfig.sample_rate = static_cast<int>(palDevice->config.sample_rate);
    aidlMediaConfig.bit_width = static_cast<int>(palDevice->config.bit_width);
    aidlChannelInfo.channels = static_cast<char>(palDevice->config.ch_info.channels);
    memcpy(aidlChannelInfo.ch_map.data(), palDevice->config.ch_info.ch_map, PAL_MAX_CHANNELS_SUPPORTED);
    aidlMediaConfig.aud_fmt_id = static_cast<PalAudioFmt>(palDevice->config.aud_fmt_id);
    aidlPalDevice.config = aidlMediaConfig;

    // AIDL address
    aidlAddress.card_id = palDevice->address.card_id;
    aidlAddress.device_num = palDevice->address.device_num;
    aidlPalDevice.address = aidlAddress;

    aidlPalDevice.sndDevName.resize(DEVICE_NAME_MAX_SIZE);
    memcpy(aidlPalDevice.sndDevName.data(), palDevice->sndDevName, DEVICE_NAME_MAX_SIZE);

    aidlPalDevice.custom_config.custom_key.resize(PAL_MAX_CUSTOM_KEY_SIZE);
    memcpy(aidlPalDevice.custom_config.custom_key.data(), palDevice->custom_config.custom_key, PAL_MAX_CUSTOM_KEY_SIZE);

    ALOGV("%s config %s", __func__, aidlPalDevice.toString().c_str());
    return aidlPalDevice;
}

ModifierKV LegacyToAidl::convertModifierKV(struct modifier_kv *modifierKv)
{
    ModifierKV aidlKv;

    if (modifierKv == nullptr) {
        return {};
    }

    aidlKv.key = modifierKv->key;
    aidlKv.value = modifierKv->value;

    ALOGV("%s config %s", __func__, aidlKv.toString().c_str());
    return aidlKv;
}

PalDrainType LegacyToAidl::convertPalDrainType(pal_drain_type_t palDrainType)
{
    if (!palDrainType) {
        return {};
    }

    return static_cast<PalDrainType>(palDrainType);
}

PalBufferConfig LegacyToAidl::convertPalBufferConfig(struct pal_buffer_config *palBufferConfig)
{
    PalBufferConfig aidlConfig;

    if (palBufferConfig == nullptr) {
        return {};
    }

    aidlConfig.buf_count = static_cast<int>(palBufferConfig->buf_count);
    aidlConfig.buf_size = static_cast<int>(palBufferConfig->buf_size);
    aidlConfig.max_metadata_size = static_cast<int>(palBufferConfig->max_metadata_size);

    return std::move(aidlConfig);
}

PalParamPayload LegacyToAidl::convertPalParamPayload(pal_param_payload *palParamPayload)
{
    PalParamPayload aidlPayload;

    if (palParamPayload == nullptr) {
        return {};
    }

    aidlPayload.payload.resize(palParamPayload->payload_size);
    memcpy(aidlPayload.payload.data(), palParamPayload->payload, palParamPayload->payload_size);

    return std::move(aidlPayload);
}

aidl::android::hardware::common::NativeHandle fdToNativeHandle(int fd, int intToCopy = -1)
{
    aidl::android::hardware::common::NativeHandle handle;
    handle.fds.emplace_back(dup(fd));
    if (intToCopy != -1) handle.ints.emplace_back(intToCopy);
    return std::move(handle);
}

PalBuffer LegacyToAidl::convertPalBuffer(struct pal_buffer *palBuffer) {
    PalBuffer aidlBuffer;
    TimeSpec aidlTimeSpec;

    if (palBuffer == nullptr) {
        return {};
    }

    aidlBuffer.size = static_cast<int>(palBuffer->size);
    aidlBuffer.offset = static_cast<int>(palBuffer->offset);
    aidlBuffer.buffer.resize(palBuffer->size);
    aidlBuffer.flags = static_cast<int>(palBuffer->flags);
    aidlBuffer.frame_index = static_cast<long>(palBuffer->frame_index);

    // AIDL Time Stamp
    if (palBuffer->ts) {
        aidlTimeSpec.tvSec = palBuffer->ts->tv_sec;
        aidlTimeSpec.tvNSec = palBuffer->ts->tv_nsec;
        aidlBuffer.timeStamp = aidlTimeSpec;
    }

    if (palBuffer->size && palBuffer->buffer) {
        memcpy(aidlBuffer.buffer.data(), palBuffer->buffer, palBuffer->size);
    }

    aidlBuffer.alloc_info.alloc_handle = fdToNativeHandle(palBuffer->alloc_info.alloc_handle, palBuffer->alloc_info.alloc_handle);
    aidlBuffer.alloc_info.alloc_size = static_cast<int>(palBuffer->alloc_info.alloc_size);
    aidlBuffer.alloc_info.offset = palBuffer->alloc_info.offset;

    return std::move(aidlBuffer);
}

void LegacyToAidl::convertPalSessionTimeToAidl(struct pal_session_time* palSessTime, PalSessionTime* aildSessTime) {
    if (palSessTime) {
        aildSessTime->session_time.valLsw = palSessTime->session_time.value_lsw;
        aildSessTime->session_time.valMsw = palSessTime->session_time.value_msw;
        aildSessTime->absolute_time.valLsw = palSessTime->absolute_time.value_lsw;
        aildSessTime->absolute_time.valMsw = palSessTime->absolute_time.value_msw;
        aildSessTime->timestamp.valLsw = palSessTime->timestamp.value_lsw;
        aildSessTime->timestamp.valMsw = palSessTime->timestamp.value_msw;
    }
}

void LegacyToAidl::convertMmapBufferInfoToAidl(struct pal_mmap_buffer* palMmapBuffer, PalMmapBuffer* aidlMmapBuffer) {
    if (palMmapBuffer) {
        aidlMmapBuffer->buffer = (uint64_t)palMmapBuffer->buffer;
        aidlMmapBuffer->fd = palMmapBuffer->fd;
        aidlMmapBuffer->buffer_size_frames = palMmapBuffer->buffer_size_frames;
        aidlMmapBuffer->burst_size_frames = palMmapBuffer->burst_size_frames;
        aidlMmapBuffer->flags = (PalMmapBufferFlags)palMmapBuffer->flags;
    }
}

void LegacyToAidl::convertMmapPositionInfoToAidl(struct pal_mmap_position* palMmapPosition, PalMmapPosition* aidlMmapPosition) {
    if (palMmapPosition) {
        aidlMmapPosition->time_nanoseconds = palMmapPosition->time_nanoseconds;
        aidlMmapPosition->position_frames = palMmapPosition->position_frames;
    }
}

PalAudioEffect LegacyToAidl::convertPalAudioEffect(pal_audio_effect_t palAudioEffect)
{
    if (!palAudioEffect) {
        return {};
    }

    return static_cast<PalAudioEffect>(palAudioEffect);
}

PalMmapBuffer LegacyToAidl::convertPalMmapBuffer(struct pal_mmap_buffer * palMmapBuffer)
{
    PalMmapBuffer aidlBuffer;

    if (palMmapBuffer == nullptr) {
        return {};
    }

    aidlBuffer.buffer = reinterpret_cast<long>(palMmapBuffer->buffer);
    aidlBuffer.fd = palMmapBuffer->fd;
    aidlBuffer.buffer_size_frames = palMmapBuffer->buffer_size_frames;
    aidlBuffer.burst_size_frames = palMmapBuffer->burst_size_frames;
    aidlBuffer.flags = static_cast<PalMmapBufferFlags>(palMmapBuffer->flags);

    return aidlBuffer;
}

PalMmapPosition LegacyToAidl::convertPalMmapPosition(struct pal_mmap_position * palMmapPosition)
{
    PalMmapPosition aidlPosition;

    if (palMmapPosition == nullptr) {
        return {};
    }

    aidlPosition.time_nanoseconds = static_cast<long>(palMmapPosition->time_nanoseconds);
    aidlPosition.position_frames = static_cast<int>(palMmapPosition->position_frames);

    return aidlPosition;
}

PalVolumeData LegacyToAidl::convertPalVolData(pal_volume_data *palVolData)
{
    PalVolumeData aidlPalVolData;

    if (palVolData == nullptr) {
        return {};
    }

    aidlPalVolData.volPair.resize(palVolData->no_of_volpair);
    memcpy(aidlPalVolData.volPair.data(), palVolData->volume_pair,
            sizeof(PalChannelVolKv) * palVolData->no_of_volpair);

    return std::move(aidlPalVolData);
}

std::vector<uint8_t> LegacyToAidl::convertRawPalParamPayloadToVector(void *payload, size_t size)
{
    if (payload == nullptr) {
        return {};
    }

    std::vector<uint8_t> aidlPayload(size);
    memcpy(aidlPayload.data(), payload, size);
    return std::move(aidlPayload);
}
}
