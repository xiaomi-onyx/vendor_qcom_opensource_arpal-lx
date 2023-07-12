/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

package vendor.qti.hardware.pal;

import vendor.qti.hardware.pal.PalCallbackBuffer;
import vendor.qti.hardware.pal.PalReadWriteDoneCommand;
import vendor.qti.hardware.pal.PalReadWriteDoneResult;
import vendor.qti.hardware.pal.PalCallbackReturnData;

@VintfStability
interface IPALCallback {
    void event_callback(in long streamHandle, in int event_id, in int event_data_size,
        in byte[] event_data, in long cookie);

    oneway void event_callback_rw_done(in long streamHandle, in int event_id,
        in int event_data_size, in PalCallbackBuffer[] rw_done_payload, in long cookie);

    PalCallbackReturnData prepare_mq_for_transfer(in long streamHandle, in long cookie);
}
