// SPDX-License-Identifier: MIT
// Copyright (c) 2021 The Pybricks Authors

/**
 * @addtogroup ResistorLadderDriver Driver: Resistor Ladder
 * @{
 */

#ifndef _PBDRV_RESISTOR_LADDER_H_
#define _PBDRV_RESISTOR_LADDER_H_

#include <stdint.h>

#include <pbio/error.h>

/** Resistor ladder input channel flags. */
typedef enum {
    /** Input channel 0. */
    PBDRV_RESISTOR_LADDER_CH_0 = 1 << 0,
    /** Input channel 1. */
    PBDRV_RESISTOR_LADDER_CH_1 = 1 << 1,
    /** Input channel 0. */
    PBDRV_RESISTOR_LADDER_CH_2 = 1 << 2,
} pbdrv_resistor_ladder_ch_flags_t;

/** Decode a three-input resistor ladder; leave flags unchanged on invalid voltage. */
static inline pbio_error_t pbdrv_resistor_ladder_decode(uint16_t value,
    const uint16_t level[8], pbdrv_resistor_ladder_ch_flags_t *flags) {
    if (value > level[0]) {
        *flags = 0;
    } else if (value > level[1]) {
        *flags = PBDRV_RESISTOR_LADDER_CH_2;
    } else if (value > level[2]) {
        *flags = PBDRV_RESISTOR_LADDER_CH_1;
    } else if (value > level[3]) {
        *flags = PBDRV_RESISTOR_LADDER_CH_1 | PBDRV_RESISTOR_LADDER_CH_2;
    } else if (value > level[4]) {
        *flags = PBDRV_RESISTOR_LADDER_CH_0;
    } else if (value > level[5]) {
        *flags = PBDRV_RESISTOR_LADDER_CH_0 | PBDRV_RESISTOR_LADDER_CH_2;
    } else if (value > level[6]) {
        *flags = PBDRV_RESISTOR_LADDER_CH_0 | PBDRV_RESISTOR_LADDER_CH_1;
    } else if (value > level[7]) {
        *flags = PBDRV_RESISTOR_LADDER_CH_0 | PBDRV_RESISTOR_LADDER_CH_1 | PBDRV_RESISTOR_LADDER_CH_2;
    } else {
        // hardware failure?
        return PBIO_ERROR_IO;
    }

    return PBIO_SUCCESS;
}

pbio_error_t pbdrv_resistor_ladder_get(uint8_t id, pbdrv_resistor_ladder_ch_flags_t *flags);

#endif // _PBDRV_RESISTOR_LADDER_H_

/** @} */
