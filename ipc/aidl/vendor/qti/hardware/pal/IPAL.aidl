/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package vendor.qti.hardware.pal;

import vendor.qti.hardware.pal.IPALCallback;
import vendor.qti.hardware.pal.ModifierKV;
import vendor.qti.hardware.pal.PalAudioEffect;
import vendor.qti.hardware.pal.PalBuffer;
import vendor.qti.hardware.pal.PalBufferConfig;
import vendor.qti.hardware.pal.PalDevice;
import vendor.qti.hardware.pal.PalDeviceId;
import vendor.qti.hardware.pal.PalDrainType;
import vendor.qti.hardware.pal.PalMmapBuffer;
import vendor.qti.hardware.pal.PalMmapPosition;
import vendor.qti.hardware.pal.PalParamPayload;
import vendor.qti.hardware.pal.PalSessionTime;
import vendor.qti.hardware.pal.PalStreamAttributes;
import vendor.qti.hardware.pal.PalStreamType;
import vendor.qti.hardware.pal.PalVolumeData;
import vendor.qti.hardware.pal.PalReadReturnData;

@VintfStability
interface IPAL {
    void ipc_pal_add_remove_effect(in long streamHandle, in PalAudioEffect effect,
        in boolean enable);

    byte[] ipc_pal_gef_rw_param(in int paramId, in byte[] param_payload,
        in PalDeviceId dev_id, in PalStreamType strm_type, in byte dir);

    boolean ipc_pal_get_mic_mute();

    byte[] ipc_pal_get_param(in int paramId);

    PalSessionTime ipc_pal_get_timestamp(in long streamHandle);

    void ipc_pal_register_global_callback(in IPALCallback cb, in long cookie);

    void ipc_pal_set_mic_mute(in boolean state);

    void ipc_pal_set_param(in int paramId, in byte[] payload);

    void ipc_pal_stream_close(in long streamHandle);

    PalMmapBuffer ipc_pal_stream_create_mmap_buffer(in long streamHandle, in int min_size_frames);

    void ipc_pal_stream_drain(in long streamHandle, in PalDrainType type);

    void ipc_pal_stream_flush(in long streamHandle);

    int ipc_pal_stream_get_buffer_size(in long streamHandle, in int in_buf_size,
        in int out_buf_size);

    PalDevice[] ipc_pal_stream_get_device(in long streamHandle);

    PalMmapPosition ipc_pal_stream_get_mmap_position(in long streamHandle);

    boolean ipc_pal_stream_get_mute(in long streamHandle);

    PalParamPayload ipc_pal_stream_get_param(in long streamHandle, in int param_id);

    byte[] ipc_pal_stream_get_tags_with_module_info(in long stream_handle, in int size);

    PalVolumeData ipc_pal_stream_get_volume(in long streamHandle);

    long ipc_pal_stream_open(in PalStreamAttributes attributes, in PalDevice[] devices,
                             in ModifierKV[] modifiers, in IPALCallback cb, in long ipc_clt_data);

    void ipc_pal_stream_pause(in long streamHandle);

    PalReadReturnData ipc_pal_stream_read(in long streamHandle, in PalBuffer[] buffer);

    void ipc_pal_stream_resume(in long streamHandle);

    PalBufferConfig[] ipc_pal_stream_set_buffer_size(in long streamHandle, in PalBufferConfig rx_config,
        in PalBufferConfig tx_config);

    void ipc_pal_stream_set_device(in long streamHandle,
        in PalDevice[] devices);

    void ipc_pal_stream_set_mute(in long streamHandle, in boolean state);

    void ipc_pal_stream_set_param(in long streamHandle, in int param_id,
                                  in PalParamPayload paramPayload);

    void ipc_pal_stream_set_volume(in long streamHandle, in PalVolumeData vol);

    void ipc_pal_stream_start(in long streamHandle);

    void ipc_pal_stream_stop(in long streamHandle);

    void ipc_pal_stream_suspend(in long streamHandle);

    int ipc_pal_stream_write(in long streamHandle, in PalBuffer[] buffer);
}
