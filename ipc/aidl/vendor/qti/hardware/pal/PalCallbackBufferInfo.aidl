/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package vendor.qti.hardware.pal;

@VintfStability
parcelable PalCallbackBufferInfo {
    long frame_index;
    int sample_rate;
    int bit_width;
    char channel_count;
}
