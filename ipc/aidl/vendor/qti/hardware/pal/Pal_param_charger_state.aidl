/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package vendor.qti.hardware.pal;

/**
 * Payload For ID: PAL_PARAM_ID_CHARGER_STATE
 *  Description   : Charger State
 */
@VintfStability
parcelable Pal_param_charger_state {
    boolean is_charger_online;
    /**
     * < status of charger
     */
    boolean is_concurrent_boost_enable;
}
