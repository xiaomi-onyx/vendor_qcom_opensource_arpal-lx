/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package vendor.qti.hardware.pal;

import vendor.qti.hardware.pal.PalAudioFmt;
import vendor.qti.hardware.pal.PalChannelInfo;

/**
 * Media configuraiton
 */
@VintfStability
parcelable PalMediaConfig {
    int sample_rate;
    int bit_width;
    PalChannelInfo ch_info;
    PalAudioFmt aud_fmt_id;
}
