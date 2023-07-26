/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once


#include <aidl/vendor/qti/hardware/pal/PalStreamAttributes.h>
#include <aidl/vendor/qti/hardware/pal/PalDevice.h>
#include <aidl/vendor/qti/hardware/pal/ModifierKV.h>
#include <aidl/vendor/qti/hardware/pal/PalDrainType.h>
#include <aidl/vendor/qti/hardware/pal/PalBufferConfig.h>
#include <aidl/vendor/qti/hardware/pal/PalBuffer.h>
#include <aidl/vendor/qti/hardware/pal/PalParamPayload.h>
#include <aidl/vendor/qti/hardware/pal/PalAudioEffect.h>
#include <aidl/vendor/qti/hardware/pal/PalMmapBuffer.h>
#include <aidl/vendor/qti/hardware/pal/PalMmapPosition.h>
#include <aidl/vendor/qti/hardware/pal/PalVolumeData.h>
#include <aidl/vendor/qti/hardware/pal/PalSessionTime.h>
#include <PalDefs.h>

namespace aidl::vendor::qti::hardware::pal {

struct LegacyToAidl {

    static PalStreamAttributes convertPalStreamAttributes(struct pal_stream_attributes *palStreamAttributes);

    static PalDevice convertPalDevice(struct pal_device *palDevice);

    static ModifierKV convertModifierKV(struct modifier_kv *modifierKV);

    static PalDrainType convertPalDrainType(pal_drain_type_t palDrainType);

    static PalBufferConfig convertPalBufferConfig(struct pal_buffer_config *palBufferConfig);

    static PalBuffer convertPalBuffer(struct pal_buffer *palBuffer);

    static PalParamPayload convertPalParamPayload(pal_param_payload *palParamPayload);

    static PalAudioEffect convertPalAudioEffect(pal_audio_effect_t effect);

    /*TODO Keeping both Defs of Mmap for now, will clear it later */
    static PalMmapBuffer convertPalMmapBuffer(struct pal_mmap_buffer * palMmapBuffer);

    static PalMmapPosition convertPalMmapPosition(struct pal_mmap_position * palMmapPosition);

    static PalVolumeData convertPalVolData(pal_volume_data *palVolData);

    static std::vector<uint8_t> convertRawPalParamPayloadToVector(void *payload, size_t size);

    static void convertPalSessionTimeToAidl(struct pal_session_time* palSessTime, PalSessionTime* aildSessTime);

    static void convertMmapBufferInfoToAidl(struct pal_mmap_buffer* palMmapBuffer, PalMmapBuffer* aidlMmapBuffer);

    static void convertMmapPositionInfoToAidl(struct pal_mmap_position *palMmapPosition,
                                              PalMmapPosition* aidlMmapPosition);
};
}
