/*
 * Copyright 2018 Eric Tremblay <habscup@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _NSEC_STORAGE_H
#define _NSEC_STORAGE_H

#include "nsec_led_ble.h"

void nsec_storage_init(void);
void nsec_storage_update(void);

void load_stored_led_settings(void);
void load_stored_led_default_settings(void);
void update_stored_brightness(uint8_t brightness);
void update_stored_mode(uint8_t mode);
void update_stored_speed(uint16_t speed);
void update_stored_color(uint32_t color, uint8_t index);
void update_stored_reverse(bool reverse);
void update_stored_control(bool control);
void update_stored_segment(SegmentBle *segment, int index);
void update_is_ble_controlled(bool ble_controlled);
void update_ble_control_permitted(bool permitted);

bool nsec_unlock_led_pattern(char *password, uint8_t index);
bool pattern_is_unlock(uint32_t sponsor_index);

void update_identity(char *new_identity);
void load_stored_identity(char *identity);

#endif