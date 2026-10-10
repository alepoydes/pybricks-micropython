// SPDX-License-Identifier: MIT
// Copyright (c) 2020-2026 The Pybricks Authors

#ifndef _PBDRV_TLC5955_H_
#define _PBDRV_TLC5955_H_

#include <stdint.h>

// size in bytes to hold 769-bit shift register data
#define TLC5955_DATA_SIZE ((769 + 7) / 8)

// number of PWM channels on TLC5955
#define TLC5955_NUM_CHANNEL 48

/** Values for TLC5955_CONTROL_DATA maximum current parameter. */
enum {
    /** Max current: 3.2 mA */
    TLC5955_MC_3_2 = 0b000,
    /** Max current: 8.0 mA */
    TLC5955_MC_8_0 = 0b001,
    /** Max current: 11.2 mA */
    TLC5955_MC_11_2 = 0b010,
    /** Max current: 15.9 mA */
    TLC5955_MC_15_9 = 0b011,
    /** Max current: 19.1 mA */
    TLC5955_MC_19_1 = 0b100,
    /** Max current: 23.9 mA */
    TLC5955_MC_23_9 = 0b101,
    /** Max current: 27.1 mA */
    TLC5955_MC_27_1 = 0b110,
    /** Max current: 31.9 mA */
    TLC5955_MC_31_9 = 0b111,
};

/**
 * Control data latch initializer
 *
 * Note: we are using this as generic PWM, so red/green/blue are all configured
 * the same.
 *
 * @param name      variable name
 * @param dc        dot correction (7-bit)
 * @param mc        maximum current (3-bit)
 * @param bc        global brightness control (7-bit)
 * @param dsprpt    Auto display repeat mode enable (1-bit)
 * @param tmgrst    Display timing reset mode enable (1-bit)
 * @param rfresh    Auto data refresh mode enable (1-bit)
 * @param espwm     ES-PWM mode enable (1-bit)
 * @param lsdvlt    LSD detection voltage selection (1-bit)
 */
#define TLC5955_CONTROL_DATA(name, dc, mc, bc, dsprpt, tmgrst, rfresh, espwm, lsdvlt) \
    uint8_t name[TLC5955_DATA_SIZE] = { \
        /* bit 768 */      [0] = 1, \
        /* bits 767-760 */ [1] = 0x96, \
        /* bits 370-368 */ [50] = (((lsdvlt) << 2) | ((espwm) << 1) | (rfresh)) & 0xff, \
        /* bits 367-360 */ [51] = (((tmgrst) << 7) | ((dsprpt) << 6) | ((bc) >> 1)) & 0xff, \
        /* bits 359-352 */ [52] = (((bc) << 7) | ((bc) >> 0)) & 0xff, \
        /* bits 351-344 */ [53] = (((bc) << 1) | ((mc) >> 2)) & 0xff, \
        /* bits 343-336 */ [54] = (((mc) << 6) | ((mc) << 3) | (mc)) & 0xff, \
        /* bits 335-328 */ [55] = (((dc) << 1) | ((dc) >> 6)) & 0xff, \
        /* bits 327-320 */ [56] = (((dc) << 2) | ((dc) >> 5)) & 0xff, \
        /* bits 319-312 */ [57] = (((dc) << 3) | ((dc) >> 4)) & 0xff, \
        /* bits 311-304 */ [58] = (((dc) << 4) | ((dc) >> 3)) & 0xff, \
        /* bits 303-296 */ [59] = (((dc) << 5) | ((dc) >> 2)) & 0xff, \
        /* bits 295-288 */ [60] = (((dc) << 6) | ((dc) >> 1)) & 0xff, \
        /* bits 287-280 */ [61] = (((dc) << 7) | ((dc) >> 0)) & 0xff, \
        /* bits 279-272 */ [62] = (((dc) << 1) | ((dc) >> 6)) & 0xff, \
        /* bits 271-264 */ [63] = (((dc) << 2) | ((dc) >> 5)) & 0xff, \
        /* bits 263-256 */ [64] = (((dc) << 3) | ((dc) >> 4)) & 0xff, \
        /* bits 255-248 */ [65] = (((dc) << 4) | ((dc) >> 3)) & 0xff, \
        /* bits 247-240 */ [66] = (((dc) << 5) | ((dc) >> 2)) & 0xff, \
        /* bits 239-232 */ [67] = (((dc) << 6) | ((dc) >> 1)) & 0xff, \
        /* bits 231-224 */ [68] = (((dc) << 7) | ((dc) >> 0)) & 0xff, \
        /* bits 223-216 */ [69] = (((dc) << 1) | ((dc) >> 6)) & 0xff, \
        /* bits 215-208 */ [70] = (((dc) << 2) | ((dc) >> 5)) & 0xff, \
        /* bits 207-200 */ [71] = (((dc) << 3) | ((dc) >> 4)) & 0xff, \
        /* bits 199-192 */ [72] = (((dc) << 4) | ((dc) >> 3)) & 0xff, \
        /* bits 191-184 */ [73] = (((dc) << 5) | ((dc) >> 2)) & 0xff, \
        /* bits 183-176 */ [74] = (((dc) << 6) | ((dc) >> 1)) & 0xff, \
        /* bits 175-168 */ [75] = (((dc) << 7) | ((dc) >> 0)) & 0xff, \
        /* bits 167-160 */ [76] = (((dc) << 1) | ((dc) >> 6)) & 0xff, \
        /* bits 159-152 */ [77] = (((dc) << 2) | ((dc) >> 5)) & 0xff, \
        /* bits 151-144 */ [78] = (((dc) << 3) | ((dc) >> 4)) & 0xff, \
        /* bits 143-136 */ [79] = (((dc) << 4) | ((dc) >> 3)) & 0xff, \
        /* bits 135-128 */ [80] = (((dc) << 5) | ((dc) >> 2)) & 0xff, \
        /* bits 127-120 */ [81] = (((dc) << 6) | ((dc) >> 1)) & 0xff, \
        /* bits 119-112 */ [82] = (((dc) << 7) | ((dc) >> 0)) & 0xff, \
        /* bits 111-104 */ [83] = (((dc) << 1) | ((dc) >> 6)) & 0xff, \
        /* bits 103-96 */  [84] = (((dc) << 2) | ((dc) >> 5)) & 0xff, \
        /* bits 95-88 */   [85] = (((dc) << 3) | ((dc) >> 4)) & 0xff, \
        /* bits 87-80 */   [86] = (((dc) << 4) | ((dc) >> 3)) & 0xff, \
        /* bits 79-72 */   [87] = (((dc) << 5) | ((dc) >> 2)) & 0xff, \
        /* bits 71-64 */   [88] = (((dc) << 6) | ((dc) >> 1)) & 0xff, \
        /* bits 63-56 */   [89] = (((dc) << 7) | ((dc) >> 0)) & 0xff, \
        /* bits 55-48 */   [90] = (((dc) << 1) | ((dc) >> 6)) & 0xff, \
        /* bits 47-40 */   [91] = (((dc) << 2) | ((dc) >> 5)) & 0xff, \
        /* bits 39-32 */   [92] = (((dc) << 3) | ((dc) >> 4)) & 0xff, \
        /* bits 31-24 */   [93] = (((dc) << 4) | ((dc) >> 3)) & 0xff, \
        /* bits 23-16 */   [94] = (((dc) << 5) | ((dc) >> 2)) & 0xff, \
        /* bits 15-8 */    [95] = (((dc) << 6) | ((dc) >> 1)) & 0xff, \
        /* bits 7-0 */     [96] = (((dc) << 7) | ((dc) >> 0)) & 0xff, \
    }

/** Set a validated channel (0..47) in a grayscale frame, in wire order. */
static inline void pbdrv_tlc5955_set_duty(uint8_t *frame, uint32_t channel, uint16_t value) {
    frame[channel * 2 + 1] = value >> 8;
    frame[channel * 2 + 2] = value;
}

#endif // _PBDRV_TLC5955_H_
