/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package vendor.qti.hardware.pal;

@VintfStability
parcelable PalStreamInfo {
    long version;
    long size;
    long duration_us;
    boolean has_video;
    boolean is_streaming;
    int loopback_type;
    int haptics_type;
}
