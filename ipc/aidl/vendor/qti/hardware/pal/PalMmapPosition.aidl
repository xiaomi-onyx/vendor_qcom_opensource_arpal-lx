/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package vendor.qti.hardware.pal;

/**
 * Mmap buffer read/write position returned by GetMmapPosition.
 * note\ Used by streams opened in mmap mode.
 */
@VintfStability
parcelable PalMmapPosition {
    long time_nanoseconds;
    /**
     * < timestamp in ns, CLOCK_MONOTONIC
     */
    int position_frames;
}
