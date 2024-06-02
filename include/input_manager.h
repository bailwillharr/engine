#pragma once

#include <array>
#include <string>
#include <vector>

#include "input_keys.h"
#include "input_mouse.h"
#include "window.h"

namespace engine {

enum class InputDevice : int { kMouse, kKeyboard, kController, kSize };

// This class should be used to get platform/input-device independent input
class InputManager {
   public:
    /* The Window object here is stored for the duration of the InputManager.
     * 'win' must point to a valid Window object. */
    InputManager(const Window* win) : win_(win)
    {
        assert(win != nullptr);
        enabled_devices_.fill(true);
    }
    InputManager(const InputManager&) = delete;

    InputManager& operator=(const InputManager&) = delete;

    ~InputManager() {}

    void AddInputButton(const std::string& name, inputs::MouseButton button) { AddInputDeviceButton(name, InputDevice::kMouse, static_cast<int>(button)); }

    void AddInputButton(const std::string& name, inputs::Key button) { AddInputDeviceButton(name, InputDevice::kKeyboard, static_cast<int>(button)); }

    void AddInputAxis(const std::string& name, inputs::MouseAxis axis) { AddInputDeviceAxis(name, InputDevice::kMouse, static_cast<int>(axis)); }

    void AddInputButtonAsAxis(const std::string& name, inputs::MouseButton high, inputs::MouseButton low)
    {
        AddInputDeviceButtonAsAxis(name, InputDevice::kMouse, static_cast<int>(high), static_cast<int>(low));
    }

    void AddInputButtonAsAxis(const std::string& name, inputs::Key high, inputs::Key low)
    {
        AddInputDeviceButtonAsAxis(name, InputDevice::kKeyboard, static_cast<int>(high), static_cast<int>(low));
    }

    void DelInputButton(int index)
    {
        std::vector<struct ButtonEntry>::iterator it = button_entries_.begin();
        std::advance(it, index);
        button_entries_.erase(it);
    }

    void DelInputAxis(int index)
    {
        std::vector<struct AxisEntry>::iterator it = axis_entries_.begin();
        std::advance(it, index);
        axis_entries_.erase(it);
    }

    void SetDeviceActive(enum InputDevice device, bool active) { enabled_devices_[static_cast<int>(device)] = active; }

    bool GetDeviceActive(enum InputDevice device) const { return enabled_devices_[static_cast<int>(device)]; }

    float GetAxis(const std::string& axis_name) const;

    bool GetButton(const std::string& button_name) const;

    bool GetButtonPress(const std::string& button_name) const;

    bool GetButtonRelease(const std::string& button_name) const;

   private:
    struct ButtonEntry {
        std::string name;
        enum InputDevice device;
        int button; // enumeration of device buttons or axes
    };

    struct AxisEntry {
        std::string name;
        enum InputDevice device;
        int axis;
        bool is_button_axis;
        int high;
        int low;
    };

    const Window* win_;

    std::vector<struct ButtonEntry> button_entries_;
    std::vector<struct AxisEntry> axis_entries_;

    std::array<bool, static_cast<int>(InputDevice::kSize)> enabled_devices_;

    // private methods

    float GetDeviceAxis(enum InputDevice device, int axis) const;

    bool GetDeviceButton(enum InputDevice device, int button) const;

    bool getDeviceButtonDown(enum InputDevice device, int button) const;

    bool GetDeviceButtonUp(enum InputDevice device, int button) const;

    float GetButtonAxis(enum InputDevice device, int high, int low) const
    {
        float value = 0.0f;
        if (GetDeviceButton(device, high)) value += 1.0f;
        if (low != 0) {
            if (GetDeviceButton(device, low)) value += -1.0f;
        }
        return value;
    }

    void AddInputDeviceButton(const std::string& name, InputDevice device, int button) { button_entries_.push_back({name, device, button}); }

    void AddInputDeviceAxis(const std::string& name, InputDevice device, int axis) { axis_entries_.push_back({name, device, axis, false, 0, 0}); }

    void AddInputDeviceButtonAsAxis(const std::string& name, InputDevice device, int high, int low)
    {
        axis_entries_.push_back({name, device, 0, true, high, low});
    }
};

} // namespace engine