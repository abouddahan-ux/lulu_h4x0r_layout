// Copyright 2022 Cole Smith <cole@boadsource.xyz>
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include QMK_KEYBOARD_H
#include "quantum.h"
#include "process_tap_dance.h"
#include "layer_lock.h"
#include "process_key_override.h"
#include "keycodes.h"
#include "print.h"

#ifdef CONSOLE_ENABLE
#define LOG(msg)       \
print(msg)
#define LOGF(msg, ...) \
printf(msg, ##__VA_ARGS__)
#else
#define LOG(msg)
#define LOGF(mg, ...)
#endif

enum layers {
    _QWERTY,
    _FUNCTION,
    _MEDIA,
    _SYMBOL,
    _NUMBER,
};

// Tap Dance Action
enum td_actions {
    TD_LAYER,
    TD_ESC_RCTL,
    TD_RLAYER,
};

/**
 * On each tap activate tapCount layer if it is not 0 or locked.
 */
void dance_layer_each(tap_dance_state_t *state, void *user_data) {
    LOG("dance_layer_each\n");

    uint8_t target_layer = state->count;
    if (target_layer > 0) {
        if (target_layer > _NUMBER) {
            LOG("reset called\n");
            reset_tap_dance(state);
        } else {
            layer_on(target_layer);
        }

        uint8_t prev_layer = target_layer - 1;
        if (prev_layer > 0 && !is_layer_locked(prev_layer)) {
            layer_off(prev_layer);
        }
    }
}

/**
 * If the dance is over, deactivates tapCount layer if it is not locked.
 */
void dance_layer_reset(tap_dance_state_t *state, void *user_data) {
    LOG("dance_layer_reset\n");

    // Implies key was held longer than tap term
    // Toggle layer
    uint8_t target_layer = state->count;
    if (target_layer > 0 && !is_layer_locked(target_layer)) {
        layer_off(target_layer);
    }
}

typedef struct {
    uint16_t tap_key;
    uint16_t hold_key;
    uint16_t registered_key;
} tap_dance_tap_hold_t;

/**
 * Determines which key to register for long holds. If first tap register hold_key else tap_key.
 */
void tap_dance_hold_finished(tap_dance_state_t *state, void *user_data) {
    LOG("tap_dance_hold_finished\n");
    tap_dance_tap_hold_t* tap_hold = (tap_dance_tap_hold_t*)user_data;

    if (state->pressed) {
        if (state->count == 1) {
            register_code16(tap_hold->hold_key);
            tap_hold->registered_key = tap_hold->hold_key;
        } else {
            register_code16(tap_hold->tap_key);
            tap_hold->registered_key = tap_hold->tap_key;
        }
    }
}

/**
 * If a held key was registered, unregister now.
 */
void tap_dance_hold_reset(tap_dance_state_t *state, void *user_data) {
    LOG("tap_dance_hold_reset\n");
    tap_dance_tap_hold_t* tap_hold = (tap_dance_tap_hold_t*)user_data;

    if (tap_hold->registered_key) {
        unregister_code16(tap_hold->registered_key);
        tap_hold->registered_key = 0;
    }
}

/**
 * Creates an action that sends tap on tap and hold on hold.
 * Long holding on a tap > 1 registers tap, otherwise reigsters hold until key is released.
 */
#define ACTION_TAP_DANCE_TAP_HOLD(tap, hold)                               \
    {                                                                      \
        .fn = {NULL, tap_dance_hold_finished, tap_dance_hold_reset, NULL}, \
        .user_data = (void*)&((tap_dance_tap_hold_t){tap, hold, 0}),       \
    }

tap_dance_action_t tap_dance_actions[] = {
    // Cycles through layers acting simillar to MO when held on the selected layer. Supports layer locking.
    [TD_LAYER] = ACTION_TAP_DANCE_FN_ADVANCED(dance_layer_each, NULL,  dance_layer_reset),
    [TD_ESC_RCTL] = ACTION_TAP_DANCE_TAP_HOLD(KC_ESC, KC_RCTL),
    [TD_RLAYER] = ACTION_TAP_DANCE_FN_ADVANCED(dance_layer_each, NULL,  dance_layer_reset),
};

const key_override_t alt_rshift_capslock_override = ko_make_basic(MOD_MASK_ALT, KC_RIGHT_SHIFT, KC_CAPS_LOCK);

const key_override_t* key_overrides[] = {
    &alt_rshift_capslock_override,
};

enum custom_keycodes {
    CKC_LAYER_LOCK = SAFE_RANGE,
};

/**
 * While TD_LAYER or TD_RLAYER active:
 *  Toggle lock on the dance layer
 * Otherwise:
 *  Toggle lock on highest active layer
 */
bool process_ckc_layer_lock(keyrecord_t *record) {
    LOG("process_ckc_layer_lock\n");
    if (!record->event.pressed) {
        return false;
    }

    tap_dance_action_t *action = &tap_dance_actions[TD_LAYER];
    uint8_t count = action->state.count;
    if (count) {
        layer_lock_invert(count);
    } else {
        uint8_t highest_layer = get_highest_layer(layer_state);
        if (highest_layer) {
            layer_lock_invert(highest_layer);
        }
    }

    return false;
}

/**
 * On hold:
 *  KC_RCTL
 * Otherwise:
 *  KC_ESC
 */
bool process_esc_rctl(keyrecord_t* record) {
    LOG("process_esc_rctl\n");
    tap_dance_action_t *action = &tap_dance_actions[TD_ESC_RCTL];
    if (!record->event.pressed && action->state.count && !action->state.finished) {
        tap_dance_tap_hold_t *tap_hold = (tap_dance_tap_hold_t*)action->user_data;
        tap_code16(tap_hold->tap_key);
    }
    return true;
}

/**
 * With ctrl:
 *  Act as CKC_LAYER_LOCK
 * Otherwise:
 i  Trigger TD_LAYER tapdance
 */
bool process_layer(keyrecord_t* record) {
    LOG("process_layer\n");
    if (get_mods() & MOD_MASK_CTRL) {
        return process_ckc_layer_lock(record);
    }
    return true;
}

/**
 * With ctrl:
 *  Trigger TD_RLAYER tapdance
 * On layer(0):
 *  Act as KC_QUOTE
 * Otherwise:
 *  Act as CKC_LAYER_LOCK
 */
bool process_r_layer(keyrecord_t* record) {
    LOG("process_r_layer\n");
    uint8_t highest_layer = get_highest_layer(layer_state);
    if (get_mods() & MOD_MASK_CTRL) {
        return true;
    }

    if (highest_layer) {
        return process_ckc_layer_lock(record);
    }

    if (record->event.pressed) {
        if (get_mods() & MOD_MASK_SHIFT) {
            register_code16(S(KC_QUOTE));
        } else {
            register_code16(KC_QUOTE);
        }
    } else {
        if (get_mods() & MOD_MASK_SHIFT) {
            unregister_code16(S(KC_QUOTE));
        } else {
            unregister_code16(KC_QUOTE);
        }
    }

    return false;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    switch (keycode) {
        case TD(TD_ESC_RCTL):
            return process_esc_rctl(record);
        case TD(TD_LAYER):
            return process_layer(record);
        case TD(TD_RLAYER):
            return process_r_layer(record);
        case CKC_LAYER_LOCK:
            return process_ckc_layer_lock(record);
    }
    return true;
}

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

/* QWERTY (1)
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * |  ~   |   1  |   2  |   3  |   4  |   5  |                    |   6  |   7  |   8  |   9  |   0  |  -   |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * | Tab  |   Q  |   W  |   E  |   R  |   T  |                    |   Y  |   U  |   I  |   O  |   P  |  =   |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * | LAYER|   A  |   S  |   D  |   F  |   G  |-------.    ,-------|   H  |   J  |   K  |   L  |   ;  |  '   |
 * |------+------+------+------+------+------|   [   |    |    ]  |------+------+------+------+------+------|
 * |LShift|   Z  |   X  |   C  |   V  |   B  |-------|    |-------|   N  |   M  |   ,  |   .  |   /  |RShift|
 * `-----------------------------------------/       /     \      \-----------------------------------------'
 *                   | LAlt | LGUI | LCtl | /Space  /       \Enter \  | ESC  |BackSP|  \   |
 *                   |      |      |      |/       /         \      \ | RCtl |      |      |
 *                   `----------------------------'           '------''--------------------'
 */
[_QWERTY] = LAYOUT(
  KC_GRAVE,    KC_1,   KC_2,    KC_3,    KC_4,    KC_5,                             KC_6,    KC_7,    KC_8,    KC_9,    KC_0,    KC_MINUS,
  KC_TAB,      KC_Q,   KC_W,    KC_E,    KC_R,    KC_T,                             KC_Y,    KC_U,    KC_I,    KC_O,    KC_P,    KC_EQUAL,
  TD(TD_LAYER), KC_A,  KC_S,    KC_D,    KC_F,    KC_G,                             KC_H,    KC_J,    KC_K,    KC_L,    KC_SCLN, TD(TD_RLAYER),
  KC_LSFT,     KC_Z,   KC_X,    KC_C,    KC_V,    KC_B, KC_LBRC, KC_RBRC,   KC_N,    KC_M,    KC_COMM, KC_DOT,      KC_SLSH,     KC_RSFT,
                                        KC_LALT, KC_LGUI, KC_LCTL,  KC_SPC,  KC_ENT,   TD(TD_ESC_RCTL), KC_BSPC, KC_BSLS
),

/* FUNCTION (2)
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * |  F1  |  F2  |  F3  |  F4  |  F5  |  F6  |                    |  F7  |  F8  |  F9  | F10  | F11  | F12  |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * | TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|                    | TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * | LAYER| TRANS| TRANS| TRANS| TRANS| TRANS|-------.    ,-------| TRANS| TRANS| TRANS| TRANS| TRANS| LOCK |
 * |------+------+------+------+------+------| TRANS |    | TRANS |------+------+------+------+------+------|
 * | TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|-------|    |-------| TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|
 * `-----------------------------------------/       /     \      \-----------------------------------------'
 *                   | TRANS| TRANS| TRANS| /TRANS  /       \TRANS \  | RCtl | DEL  | RAlt |
 *                   |      |      |      |/       /         \      \ |      |      |      |
 *                   `----------------------------'           '------''--------------------'
 */
[_FUNCTION] = LAYOUT(
  KC_F1,        KC_F2,   KC_F3,   KC_F4,   KC_F5,   KC_F6,                             KC_F7,   KC_F8,   KC_F9,   KC_F10,  KC_F11,  KC_F12,
  KC_TRNS,      KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                           KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
  TD(TD_LAYER), KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                           KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, TD(TD_RLAYER),
  KC_TRNS,      KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
                                              KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_RCTL, KC_DEL, KC_RALT
),

/* MEDIA (3)
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * | TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|                    | TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |      | VOL- | VOL+ | MUTE |      |                    | PRINT|      |  UP  |      |      |      |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * | LAYER|      | RGB- | RGB+ | RGBT |      |-------.    ,-------| PGUP | LEFT | DOWN | RIGHT|      | LOCK |
 * |------+------+------+------+------+------| HOME  |    |  END  |------+------+------+------+------+------|
 * |      |      |      |      |      |      |-------|    |-------|PGDOWN|      |      |      |      |      |
 * `-----------------------------------------/       /     \      \-----------------------------------------'
 *                   | TRANS| TRANS| TRANS| /TRANS  /       \TRANS \  | TRANS| TRANS| TRANS|
 *                   |      |      |      |/       /         \      \ |      |      |      |
 *                   `----------------------------'           '------''--------------------'
 */
[_MEDIA] = LAYOUT(
  KC_TRNS,      KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                                  KC_TRNS,         KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS, KC_TRNS,
  KC_NO,        KC_NO,   KC_VOLD, KC_VOLU, KC_MUTE, KC_NO,                                    KC_PRINT_SCREEN, KC_NO,   KC_UP,   KC_NO,    KC_NO,   KC_NO,
  TD(TD_LAYER), KC_NO,   RM_VALD, RM_VALU, RM_TOGG, KC_NO,                                    KC_PGUP,         KC_LEFT, KC_DOWN, KC_RIGHT, KC_NO,   TD(TD_RLAYER),
  KC_NO,        KC_NO,   KC_NO,   KC_NO,   KC_NO,   KC_NO,           KC_HOME, KC_END, KC_PGDN,         KC_NO,   KC_NO,   KC_NO,        KC_NO,       KC_NO,
                                                      KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
),

/* SYMBOL (4)
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * | TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|                    | TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * |      |      |   !  |   @  |   #  |      |                    | TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * | LAYER|      |   $  |   %  |   ^  |      |-------.    ,-------| TRANS| TRANS| TRANS| TRANS| TRANS| LOCK |
 * |------+------+------+------+------+------| TRANS |    | TRANS |------+------+------+------+------+------|
 * |      |      |   &  |   *  |   (  |   )  |-------|    |-------| TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|
 * `-----------------------------------------/       /     \      \-----------------------------------------'
 *                   | TRANS| TRANS| TRANS| /TRANS  /       \TRANS \  | TRANS| TRANS| TRANS|
 *                   |      |      |      |/       /         \      \ |      |      |      |
 *                   `----------------------------'           '------''--------------------'
 */
[_SYMBOL] = LAYOUT(
  KC_TRNS,      KC_TRNS, KC_TRNS,   KC_TRNS, KC_TRNS, KC_TRNS,                           KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
  KC_NO,        KC_NO,   KC_EXLM,   KC_AT,   KC_HASH, KC_TRNS,                           KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,
  TD(TD_LAYER), KC_TRNS, KC_DOLLAR, KC_PERC, KC_CIRC, KC_TRNS,                           KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, TD(TD_RLAYER),
  KC_TRNS,      KC_TRNS, KC_AMPR,   KC_ASTR, KC_LPRN, KC_RPRN, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,     KC_TRNS,     KC_TRNS,
                                              KC_TRNS, KC_TRNS, KC_TRNS,   KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
),

/* NUMBER (5)
 * ,-----------------------------------------.                    ,-----------------------------------------.
 * | TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|                    | TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * | TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|                    |   *  |   7  |   8  |   9  |   -  |      |
 * |------+------+------+------+------+------|                    |------+------+------+------+------+------|
 * | LAYER| TRANS| TRANS| TRANS| TRANS| TRANS|-------.    ,-------|   0  |   4  |   5  |   6  |   +  | LOCK |
 * |------+------+------+------+------+------| TRANS |    | TRANS |------+------+------+------+------+------|
 * | TRANS| TRANS| TRANS| TRANS| TRANS| TRANS|-------|    |-------|   /  |   1  |   2  |   3  |   =  |      |
 * `-----------------------------------------/       /     \      \-----------------------------------------'
 *                   | TRANS| TRANS| TRANS| /TRANS  /       \TRANS \  | TRANS| TRANS| TRANS|
 *                   |      |      |      |/       /         \      \ |      |      |      |
 *                   `----------------------------'           '------''--------------------'
 */
[_NUMBER] = LAYOUT(
  KC_TRNS,      KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                           KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,  KC_TRNS,
  KC_TRNS,      KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                           KC_ASTR, KC_7,    KC_8,    KC_9,    KC_MINUS, KC_NO,
  TD(TD_LAYER), KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                           KC_0,    KC_4,    KC_5,    KC_6,    KC_PLUS,  TD(TD_RLAYER),
  KC_TRNS,      KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_SLSH, KC_1,    KC_2,    KC_3,        KC_EQUAL,     KC_NO,
                                              KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS
)};
