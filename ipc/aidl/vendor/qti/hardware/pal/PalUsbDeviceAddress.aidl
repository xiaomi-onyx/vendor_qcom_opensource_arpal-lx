/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package vendor.qti.hardware.pal;

@VintfStability
parcelable PalUsbDeviceAddress {
    int card_id;
    int device_num;
}
