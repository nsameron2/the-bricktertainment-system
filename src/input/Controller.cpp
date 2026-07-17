#include "input/Controller.h"

namespace {

constexpr uint8_t CONTROLLER_STROBE_MASK = 0x01;
constexpr uint8_t BUTTON_PRESSED_MASK = 0x01;
constexpr uint8_t SERIAL_READ_FILL_MASK = 0x80;

}

void Controller::setButton(Button button, bool pressed) {
    const uint8_t mask = static_cast<uint8_t>(1 << static_cast<uint8_t>(button));

    if (pressed) {
        buttonState |= mask;
    } else {
        buttonState &= static_cast<uint8_t>(~mask);
    }

    if (strobe) {
        latchButtons();
    }
}

void Controller::setButtonState(uint8_t state) {
    buttonState = state;

    if(strobe) {
        latchButtons();
    }
}

uint8_t Controller::getButtonState() const {
    return buttonState;
}

void Controller::write(uint8_t data) {
    strobe = (data & CONTROLLER_STROBE_MASK) != 0x00;

    if (strobe) {
        latchButtons();
    }
}

uint8_t Controller::read() {
    if (strobe) {
        latchButtons();
    }

    const uint8_t data = shiftRegister & BUTTON_PRESSED_MASK;

    if (!strobe) {
        shiftRegister = static_cast<uint8_t>((shiftRegister >> 1) | SERIAL_READ_FILL_MASK);
    }

    return data;
}

void Controller::latchButtons() {
    shiftRegister = buttonState;
}
