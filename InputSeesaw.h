#pragma once
// ESP32 / Arduino only. Adafruit I2C seesaw gamepad -> the shared Input
// interface. Emits raw joystick axes (dead-zone/dominant-axis logic lives in
// the shared App) and debounced button edge events with long-press timing.
#include <Arduino.h>
#include <Adafruit_seesaw.h>
#include <Wire.h>
#include <deque>
#include "Input.h"

class InputSeesaw : public Input {
public:
    // Standard Adafruit mini gamepad mapping.
    static constexpr uint8_t PIN_X = 6, PIN_Y = 2, PIN_A = 5, PIN_B = 1,
                             PIN_SELECT = 0, PIN_START = 16;
    static constexpr uint8_t JOY_X = 14, JOY_Y = 15;

    bool begin(uint8_t addr = 0x50) {
        lastProbeMs_ = millis();
        addr_ = addr;
#ifndef ONSTEP_I2C_SDA
#define ONSTEP_I2C_SDA 44
#endif
#ifndef ONSTEP_I2C_SCL
#define ONSTEP_I2C_SCL 43
#endif
        // The generic ESP32-S3 board definition does not always choose the
        // pins used by the wiring. Probe the two common exposed pairs.
        const int pins[][2] = {{ONSTEP_I2C_SDA, ONSTEP_I2C_SCL}, {8, 9}};
        for (const auto& pair : pins) {
            Wire.end();
            Wire.begin(pair[0], pair[1], 100000);
            delay(10);
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                sda_ = pair[0]; scl_ = pair[1];
                available_ = ss_.begin(addr);
                if (available_) break;
            }
        }
        pinMode(0, INPUT_PULLUP);   // enclosure button 1 (BOOT) -> A/change
        pinMode(14, INPUT_PULLUP);  // enclosure button 2 -> power key
        if (!available_) return false;
        uint32_t pinMask = mask();
        ss_.pinModeBulk(pinMask, INPUT_PULLUP);
        return true;
    }

    bool available() const { return available_; }
    int sda() const { return sda_; }
    int scl() const { return scl_; }

    void update() override {
        edgeLocal(Button::A, 0, 6);
        edgeLocal(Button::Power, 14, 7);

        // The gamepad is optional during UI/network bring-up.  When it is not
        // connected, simply report a centered stick and no button events.
        if (!available_) {
            jx_ = 0.0f;
            jy_ = 0.0f;
            if (millis() - lastProbeMs_ >= 5000) begin(addr_);
            return;
        }

        // Joystick: 0..1023, centered ~512, invert Y so up is +.
        int rx = 1023 - ss_.analogRead(JOY_X);
        int ry = 1023 - ss_.analogRead(JOY_Y);
        jx_ =  (rx - 512) / 512.0f;
        jy_ =  (ry - 512) / 512.0f;

        uint32_t bits = ss_.digitalReadBulk(mask());  // pressed == 0 (pull-up)
        edge(Button::A,      PIN_A,      bits);
        edge(Button::B,      PIN_B,      bits);
        edge(Button::X,      PIN_X,      bits);
        edge(Button::Y,      PIN_Y,      bits);
        edge(Button::Start,  PIN_START,  bits);
        edge(Button::Select, PIN_SELECT, bits);
    }

    float joyX() const override { return jx_; }
    float joyY() const override { return jy_; }
    bool  nextButton(ButtonEvent& out) override {
        if (q_.empty()) return false;
        out = q_.front(); q_.pop_front(); return true;
    }
    bool quitRequested() const override { return false; }

private:
    static constexpr uint32_t LONG_MS = 600;
    uint32_t mask() const {
        return (1UL << PIN_X) | (1UL << PIN_Y) | (1UL << PIN_A) |
               (1UL << PIN_B) | (1UL << PIN_SELECT) | (1UL << PIN_START);
    }
    void edge(Button b, uint8_t pin, uint32_t bits) {
        bool down = !((bits >> pin) & 1);
        int i = (int)b;
        if (down && downAt_[i] == 0) {
            downAt_[i] = millis();
        } else if (!down && downAt_[i] != 0) {
            uint32_t held = millis() - downAt_[i];
            downAt_[i] = 0;
            ButtonEvent ev; ev.button = b; ev.longPress = (held >= LONG_MS);
            q_.push_back(ev);
        }
    }
    void edgeLocal(Button b, uint8_t pin, int stateIndex) {
        bool down = digitalRead(pin) == LOW;
        if (down && downAt_[stateIndex] == 0) {
            downAt_[stateIndex] = millis();
        } else if (!down && downAt_[stateIndex] != 0) {
            uint32_t held = millis() - downAt_[stateIndex];
            downAt_[stateIndex] = 0;
            ButtonEvent ev; ev.button = b; ev.longPress = held >= LONG_MS;
            q_.push_back(ev);
        }
    }

    Adafruit_seesaw ss_;
    bool available_ = false;
    int sda_ = -1, scl_ = -1;
    uint8_t addr_ = 0x50;
    uint32_t lastProbeMs_ = 0;
    std::deque<ButtonEvent> q_;
    uint32_t downAt_[8] = {};
    float jx_ = 0, jy_ = 0;
};
