#pragma once

// Physical buttons on the Adafruit seesaw gamepad (and the sim keyboard).
enum class Button { A, B, X, Y, Start, Select };

struct ButtonEvent {
    Button button;
    bool   longPress = false;   // held past the long-press threshold
};

// Abstract input. The platform layer exposes RAW joystick axes only;
// the dead-zone + dominant-axis logic lives in shared App code so the
// keyboard sim and the seesaw produce identical behaviour.
class Input {
public:
    virtual ~Input() = default;

    virtual void  update() = 0;            // pump platform events once per frame
    virtual float joyX() const = 0;        // raw, -1..1
    virtual float joyY() const = 0;        // raw, -1..1  (+ = up/North)
    virtual bool  nextButton(ButtonEvent& out) = 0;  // drain edge events
    virtual bool  quitRequested() const = 0;
};
