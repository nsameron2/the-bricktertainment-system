#pragma once

#include <cstdint>


class Controller {
public:
    enum class Button : uint8_t {
        A = 0,
        B,
        Select,
        Start,
        Up,
        Down,
        Left,
        Right,
    };

    void setButton(Button button, bool pressed);
    void setButtonState(uint8_t state);
    uint8_t getButtonState() const;

    // CPU write to 0x4016 controls the controller strobe.
    void write(uint8_t data);

    // CPU read from 0x4016/0x4017 returns one serial button bit.
    uint8_t read();

private:
    static constexpr uint8_t BUTTON_COUNT = 8;
    static constexpr uint8_t SERIAL_READ_COMPLETE_VALUE = 0x01;

    uint8_t buttonState = 0x00;
    uint8_t shiftRegister = 0x00;
    bool strobe = false;

    void latchButtons();
};
