/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package vendor.qti.hardware.pal;

@VintfStability
parcelable PalBufferConfig {
    int buf_count;
    /**
     * < number of buffers
     */
    int buf_size;
    /**
     * < This would be the size of each buffer
     */
    int max_metadata_size;
}
