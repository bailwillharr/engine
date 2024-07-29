#include "input_manager.h"

#include "window.h"

#include <string>
#include <stdexcept>

namespace engine {

float InputManager::GetAxis(const std::string& axis_name) const
{
	for (const AxisEntry& e : axis_entries_) {
		if (e.name == axis_name) {
			if (enabled_devices_[static_cast<int>(e.device)]) {
				if (e.is_button_axis) {
					return GetButtonAxis(e.device, e.high, e.low);
				}
				else {
					return GetDeviceAxis(e.device, e.axis);
				}
			}
		}
	}
	return 0.0f; // instead of throwing an exception, just return nothing
}

bool InputManager::GetButton(const std::string& buttonName) const
{
	bool isDown = false;

	for (const ButtonEntry& e : button_entries_) {
		if (e.name == buttonName) {
			if (enabled_devices_[static_cast<int>(e.device)]) {
				if (GetDeviceButton(e.device, e.button) == true) {
					isDown = true;
					break;
				}
			}
		}
	}
	return isDown;
}

bool InputManager::GetButtonPress(const std::string& buttonName) const
{
	bool isPressed = false;

	for (const ButtonEntry& e : button_entries_) {
		if (e.name == buttonName) {
			if (enabled_devices_[static_cast<int>(e.device)]) {
				if (getDeviceButtonDown(e.device, e.button) == true) {
					isPressed = true;
					break;
				}
			}
		}
	}
	return isPressed;
}

bool InputManager::GetButtonRelease(const std::string& buttonName) const
{
	bool isReleased = false;

	for (const ButtonEntry& e : button_entries_) {
		if (e.name == buttonName) {
			if (enabled_devices_[static_cast<int>(e.device)]) {
				if (GetDeviceButtonUp(e.device, e.button) == true) {
					isReleased = true;
					break;
				}
			}
		}
	}
	return isReleased;
}

float InputManager::GetDeviceAxis(enum InputDevice device, int axis) const
{
	switch (device) {
	case InputDevice::kMouse:
		switch (static_cast<inputs::MouseAxis>(axis)) {
		case inputs::MouseAxis::X:
			return static_cast<float>(m_win.GetMouseDX());
		case inputs::MouseAxis::Y:
			return static_cast<float>(m_win.GetMouseDY());
		case inputs::MouseAxis::X_SCR:
            return m_win.GetMouseScrollX();
		case inputs::MouseAxis::Y_SCR:
            return m_win.GetMouseScrollY();
		default: break;
		}
		break;
	case InputDevice::kKeyboard:
		break;
	case InputDevice::kController:
		break;
	default: break;
	}
	throw std::runtime_error("Error getting device axis");
}

bool InputManager::GetDeviceButton(enum InputDevice device, int button) const
{
	switch (device) {
	case InputDevice::kMouse:
        return m_win.GetButton(static_cast<inputs::MouseButton>(button));
	case InputDevice::kKeyboard:
        return m_win.GetKey(static_cast<inputs::Key>(button));
	case InputDevice::kController:
		break;
	default: break;
	}
	throw std::runtime_error("Error getting device button");
}

bool InputManager::getDeviceButtonDown(enum InputDevice device, int button) const
{
	switch (device) {
	case InputDevice::kMouse:
        return m_win.GetButtonPress(static_cast<enum inputs::MouseButton>(button));
	case InputDevice::kKeyboard:
        return m_win.GetKeyPress(static_cast<enum inputs::Key>(button));
	case InputDevice::kController:
		break;
	default: break;
	}
	throw std::runtime_error("Error getting device button");
}

bool InputManager::GetDeviceButtonUp(enum InputDevice device, int button) const
{
	switch (device) {
	case InputDevice::kMouse:
        return m_win.GetButtonRelease(static_cast<enum inputs::MouseButton>(button));
	case InputDevice::kKeyboard:
        return m_win.GetKeyRelease(static_cast<enum inputs::Key>(button));
	case InputDevice::kController:
		break;
	default: break;
	}
	throw std::runtime_error("Error getting device button");
}


}