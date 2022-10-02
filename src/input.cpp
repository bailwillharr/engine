#include "input.hpp"

#include "window.hpp"

#include <string>
#include <stdexcept>

namespace engine {

	Input::Input(const Window& win) : m_win(win)
	{
		m_enabledDevices.fill(true);
	}

	Input::~Input()
	{
	}

	// private methods

	float Input::getDeviceAxis(enum InputDevice device, int axis) const
	{
		switch (device) {
		case InputDevice::MOUSE:
			switch (static_cast<inputs::MouseAxis>(axis)) {
			case inputs::MouseAxis::X:
				return static_cast<float>(m_win.getMouseDX());
			case inputs::MouseAxis::Y:
				return static_cast<float>(m_win.getMouseDY());
			case inputs::MouseAxis::X_SCR:
				return m_win.getMouseScrollX();
			case inputs::MouseAxis::Y_SCR:
				return m_win.getMouseScrollY();
			default: break;
			}
			break;
		case InputDevice::KEYBOARD:
			break;
		case InputDevice::CONTROLLER:
			break;
		default: break;
		}
		throw std::runtime_error("Error getting device axis");
	}

	bool Input::getDeviceButton(enum InputDevice device, int button) const
	{
		switch (device) {
		case InputDevice::MOUSE:
			return m_win.getButton(static_cast<inputs::MouseButton>(button));
		case InputDevice::KEYBOARD:
			return m_win.getKey(static_cast<inputs::Key>(button));
		case InputDevice::CONTROLLER:
			break;
		default: break;
		}
		throw std::runtime_error("Error getting device button");
	}

	bool Input::getDeviceButtonDown(enum InputDevice device, int button) const
	{
		switch (device) {
		case InputDevice::MOUSE:
			return m_win.getButtonPress(static_cast<enum inputs::MouseButton>(button));
		case InputDevice::KEYBOARD:
			return m_win.getKeyPress(static_cast<enum inputs::Key>(button));
		case InputDevice::CONTROLLER:
			break;
		default: break;
		}
		throw std::runtime_error("Error getting device button");
	}

	bool Input::getDeviceButtonUp(enum InputDevice device, int button) const
	{
		switch (device) {
		case InputDevice::MOUSE:
			return m_win.getButtonRelease(static_cast<enum inputs::MouseButton>(button));
		case InputDevice::KEYBOARD:
			return m_win.getKeyRelease(static_cast<enum inputs::Key>(button));
		case InputDevice::CONTROLLER:
			break;
		default: break;
		}
		throw std::runtime_error("Error getting device button");
	}

	float Input::getButtonAxis(enum InputDevice device, int high, int low) const
	{
		float value = 0.0f;
		if (getDeviceButton(device, high)) value += 1.0f;
		if (low != 0) {
			if (getDeviceButton(device, low)) value += -1.0f;
		}
		return value;
	}

	// public methods

	void Input::addInputButton(const std::string& name, InputDevice device, int button)
	{
		m_buttonEntries.push_back({ name, device, button });
	}

	void Input::addInputAxis(const std::string& name, InputDevice device, int axis)
	{
		m_axisEntries.push_back({ name, device, axis, false, 0, 0 });
	}

	void Input::addInputButtonAsAxis(const std::string& name, InputDevice device, int high, int low)
	{
		m_axisEntries.push_back({ name, device, 0, true, high, low });
	}

	// OVERLOADS:

	// Add a mouse input
	void Input::addInputButton(const std::string& name, inputs::MouseButton button)
	{
		addInputButton(name, InputDevice::MOUSE, static_cast<int>(button));
	}

	void Input::addInputAxis(const std::string& name, inputs::MouseAxis axis)
	{
		addInputAxis(name, InputDevice::MOUSE, static_cast<int>(axis));
	}

	void Input::addInputButtonAsAxis(const std::string& name, inputs::MouseButton high, inputs::MouseButton low)
	{
		addInputButtonAsAxis(name, InputDevice::MOUSE, static_cast<int>(high), static_cast<int>(low));
	}

	// Add a keyboard input (TODO: add KeyboardButton enum class)
	void Input::addInputButton(const std::string& name, inputs::Key button)
	{
		addInputButton(name, InputDevice::KEYBOARD, static_cast<int>(button));
	}

	void Input::addInputButtonAsAxis(const std::string& name, inputs::Key high, inputs::Key low)
	{
		addInputButtonAsAxis(name, InputDevice::KEYBOARD, static_cast<int>(high), static_cast<int>(low));
	}

	void Input::delInputButton(int index)
	{
		std::vector<struct ButtonEntry>::iterator it = m_buttonEntries.begin();
		std::advance(it, index);
		m_buttonEntries.erase(it);
	}

	void Input::delInputAxis(int index)
	{
		std::vector<struct AxisEntry>::iterator it = m_axisEntries.begin();
		std::advance(it, index);
		m_axisEntries.erase(it);
	}

	void Input::setDeviceActive(enum InputDevice device, bool active)
	{
		m_enabledDevices[static_cast<int>(device)] = active;
	}

	bool Input::getDeviceActive(enum InputDevice device) const
	{
		return m_enabledDevices[static_cast<int>(device)];
	}

	float Input::getAxis(const std::string& axisName) const
	{
		for (const AxisEntry& e : m_axisEntries) {
			if (e.name == axisName) {
				if (m_enabledDevices[static_cast<int>(e.device)]) {
					if (e.isButtonAxis) {
						return getButtonAxis(e.device, e.high, e.low);
					}
					else {
						return getDeviceAxis(e.device, e.axis);
					}
				}
			}
		}
		return 0.0f; // instead of throwing an exception, just return nothing
	//	throw std::runtime_error("Unable to find mapping in input table");
	}

	bool Input::getButton(const std::string& buttonName) const
	{
		bool isDown = false;

		for (const ButtonEntry& e : m_buttonEntries) {
			if (e.name == buttonName) {
				if (m_enabledDevices[static_cast<int>(e.device)]) {
					if (getDeviceButton(e.device, e.button) == true) {
						isDown = true;
						break;
					}
				}
			}
		}
		return isDown;
	}

	bool Input::getButtonPress(const std::string& buttonName) const
	{
		bool isPressed = false;

		for (const ButtonEntry& e : m_buttonEntries) {
			if (e.name == buttonName) {
				if (m_enabledDevices[static_cast<int>(e.device)]) {
					if (getDeviceButtonDown(e.device, e.button) == true) {
						isPressed = true;
						break;
					}
				}
			}
		}
		return isPressed;
	}

	bool Input::getButtonRelease(const std::string& buttonName) const
	{
		bool isReleased = false;

		for (const ButtonEntry& e : m_buttonEntries) {
			if (e.name == buttonName) {
				if (m_enabledDevices[static_cast<int>(e.device)]) {
					if (getDeviceButtonUp(e.device, e.button) == true) {
						isReleased = true;
						break;
					}
				}
			}
		}
		return isReleased;
	}

}