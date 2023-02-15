#include "window.hpp"

#include <iostream>
#include <stdexcept>

static const uint64_t BILLION = 1000000000;

namespace engine {

	Window::Window(const std::string& title, bool resizable, bool fullscreen)
		: m_title(title), m_resizable(resizable), m_fullscreen(fullscreen)
	{

		// init SDL
		if (SDL_Init(SDL_INIT_VIDEO) != 0) {
			const std::string errMsg("Unable to initialise SDL: " + std::string(SDL_GetError()));
			throw std::runtime_error(errMsg);
		}

		m_counterFreq = SDL_GetPerformanceFrequency();
		m_startTime = getNanos();
		m_lastFrameStamp = m_startTime - 1;
		m_avgFpsStart = m_startTime;

		Uint32 windowFlags = SDL_WINDOW_SHOWN;

#ifdef ENGINE_BUILD_VULKAN
		windowFlags |= SDL_WINDOW_VULKAN;
#endif
#ifdef ENGINE_BUILD_OPENGL
		windowFlags |= SDL_WINDOW_OPENGL;
#endif

		if (m_resizable) {
			windowFlags |= SDL_WINDOW_RESIZABLE;
		}

		if (m_fullscreen) {
			windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		}

		// create the window
		m_handle = SDL_CreateWindow(
			m_title.c_str(),
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			static_cast<int>(m_winSize.x),
			static_cast<int>(m_winSize.y),
			windowFlags);
		if (m_handle == NULL) {
			SDL_Quit();
			throw std::runtime_error("Unable to create window: " + std::string(SDL_GetError()));
		}

		const int WINDOWED_MIN_WIDTH = 640;
		const int WINDOWED_MIN_HEIGHT = 480;
		SDL_SetWindowMinimumSize(m_handle, WINDOWED_MIN_WIDTH, WINDOWED_MIN_HEIGHT);

		// get window size
		int winWidth, winHeight;
		SDL_GetWindowSize(m_handle, &winWidth, &winHeight);

		onResize(winWidth, winHeight);

	}

	Window::~Window()
	{
		SDL_DestroyWindow(m_handle);
		SDL_Quit();
	}

	// private methods

	void Window::onResize(Sint32 width, Sint32 height)
	{
		// get window size
		m_winSize.x = static_cast<int>(width);
		m_winSize.y = static_cast<int>(height);

		m_justResized = true;
	}

	void Window::resetInputDeltas()
	{
		m_justResized = false;

		m_keyboard.deltas.fill(ButtonDelta::SAME);

		m_mouse.deltas.fill(ButtonDelta::SAME);
		m_mouse.dx = 0;
		m_mouse.dy = 0;
		m_mouse.xscroll = 0.0f;
		m_mouse.yscroll = 0.0f;
	}

	// TODO event methods (like callbacks)

	void Window::onWindowEvent(SDL_WindowEvent& e)
	{

		switch (e.event) {
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			onResize(e.data1, e.data2);
			break;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			m_keyboardFocus = true;
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			m_keyboardFocus = false;
			break;
		}
	}

	void Window::onKeyEvent(SDL_KeyboardEvent& e)
	{
		bool keyWasDown = m_keyboard.keys[e.keysym.scancode];
		bool keyIsDown = (e.state == SDL_PRESSED);
		m_keyboard.keys[e.keysym.scancode] = keyIsDown;
		if (keyIsDown != keyWasDown) { // (if key was pressed or released)
			m_keyboard.deltas[e.keysym.scancode] = keyIsDown ? ButtonDelta::PRESSED : ButtonDelta::RELEASED;
		}
	}

	void Window::onMouseButtonEvent(SDL_MouseButtonEvent& e)
	{
		enum inputs::MouseButton button = inputs::MouseButton::M_INVALID;
		switch (e.button) {
		case SDL_BUTTON_LEFT:
			button = inputs::MouseButton::M_LEFT;
			break;
		case SDL_BUTTON_MIDDLE:
			button = inputs::MouseButton::M_MIDDLE;
			break;
		case SDL_BUTTON_RIGHT:
			button = inputs::MouseButton::M_RIGHT;
			break;
		case SDL_BUTTON_X1:
			button = inputs::MouseButton::M_X1;
			break;
		case SDL_BUTTON_X2:
			button = inputs::MouseButton::M_X2;
			break;
		}
		int buttonIndex = static_cast<int>(button);
		bool buttonWasDown = m_mouse.buttons.at(buttonIndex);
		bool buttonIsDown = (e.state == SDL_PRESSED);
		m_mouse.buttons.at(buttonIndex) = buttonIsDown;
		if (buttonIsDown != buttonWasDown) { // (if button was pressed or released)
			// only sets delta if it hasn't already been set this frame (to detect very fast presses)
			if (m_mouse.deltas[buttonIndex] == ButtonDelta::SAME) {
				m_mouse.deltas[buttonIndex] = buttonIsDown ? ButtonDelta::PRESSED : ButtonDelta::RELEASED;
			}
		}
	}

	void Window::onMouseMotionEvent(SDL_MouseMotionEvent& e)
	{
		m_mouse.x = e.x;
		m_mouse.y = e.y;
		m_mouse.dx = e.xrel;
		m_mouse.dy = e.yrel;
	}

	void Window::onMouseWheelEvent(SDL_MouseWheelEvent& e)
	{
		if (e.direction == SDL_MOUSEWHEEL_NORMAL) {
			m_mouse.xscroll = e.preciseX;
			m_mouse.yscroll = e.preciseY;
		}
		else { // flipped
			m_mouse.xscroll = -e.preciseX;
			m_mouse.yscroll = -e.preciseY;
		}
	}

	// public methods

	SDL_Window* Window::getHandle() const
	{
		return m_handle;
	}

	std::string Window::getTitle() const
	{
		return m_title;
	}

	void Window::getInputAndEvents()
	{

		m_frames++;
		uint64_t currentFrameStamp = getNanos();
		m_lastFrameTime = currentFrameStamp - m_lastFrameStamp;
		m_lastFrameStamp = currentFrameStamp;

		resetInputDeltas();

		// loop through all available events
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			switch (e.type) {

			case SDL_QUIT:
				setCloseFlag();
				break;

			case SDL_WINDOWEVENT:
				onWindowEvent(e.window);
				break;

			case SDL_KEYDOWN: // FALL THROUGH
			case SDL_KEYUP:
				onKeyEvent(e.key);
				break;

			case SDL_MOUSEBUTTONDOWN: // FALL THROUGH
			case SDL_MOUSEBUTTONUP:
				onMouseButtonEvent(e.button);
				break;

			case SDL_MOUSEMOTION:
				onMouseMotionEvent(e.motion);
				break;

			case SDL_MOUSEWHEEL:
				onMouseWheelEvent(e.wheel);
				break;

			}
		}

	}

	void Window::setTitle(std::string title)
	{
		SDL_SetWindowTitle(m_handle, title.c_str());
	}

	bool Window::getWindowResized() const
	{
		return m_justResized;
	}

	void Window::setResizedFlag()
	{
		m_justResized = true;
	}

	void Window::show()
	{
		SDL_ShowWindow(m_handle);
	}

	void Window::hide()
	{
		SDL_HideWindow(m_handle);
	}

	void Window::focus()
	{
		SDL_RaiseWindow(m_handle);
		m_keyboardFocus = true;
	}

	bool Window::hasFocus() const
	{
		return m_keyboardFocus;
	}

	void Window::setCloseFlag()
	{
		m_shouldClose = true;
	}

	bool Window::isRunning() const
	{
		return !m_shouldClose;
	}

	void Window::setFullscreen(bool fullscreen, bool exclusive)
	{

		if (m_resizable) {

			SDL_DisplayMode mode;
			SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(m_handle), &mode);
			SDL_SetWindowDisplayMode(m_handle, &mode);

			if (SDL_SetWindowFullscreen(m_handle, fullscreen ? (exclusive ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP) : 0) != 0) {
				throw std::runtime_error("Unable to set window to fullscreen/windowed");
			}
			m_fullscreen = fullscreen;
			if (fullscreen) {

				int width, height;
				SDL_GetWindowSize(m_handle, &width, &height);
				onResize(width, height);
			}
		}
	}

	void Window::toggleFullscreen()
	{
		setFullscreen(!m_fullscreen, true);
	}

	bool Window::isFullscreen() const
	{
		return m_fullscreen;
	}

	bool Window::setRelativeMouseMode(bool enabled)
	{
		m_mouse.captured = enabled;
		int code = SDL_SetRelativeMouseMode(static_cast<SDL_bool>(enabled));
		if (code != 0) {
			throw std::runtime_error("Unable to set relative mouse mode");
		}
		else {
			return true;
		}
	}

	bool Window::mouseCaptured()
	{
		return m_mouse.captured;
	}

	// getting input

	bool Window::getKey(inputs::Key key) const
	{
		return m_keyboard.keys[static_cast<int>(key)];
	}

	bool Window::getKeyPress(inputs::Key key) const
	{
		return m_keyboard.deltas[static_cast<int>(key)] == ButtonDelta::PRESSED;
	}

	bool Window::getKeyRelease(inputs::Key key) const
	{
		return m_keyboard.deltas[static_cast<int>(key)] == ButtonDelta::RELEASED;
	}

	// TODO mouse input

	bool Window::getButton(inputs::MouseButton button) const
	{
		return m_mouse.buttons[static_cast<int>(button)];
	}

	bool Window::getButtonPress(inputs::MouseButton button) const
	{
		return m_mouse.deltas[static_cast<int>(button)] == ButtonDelta::PRESSED;
	}

	bool Window::getButtonRelease(inputs::MouseButton button) const
	{
		return m_mouse.deltas[static_cast<int>(button)] == ButtonDelta::RELEASED;
	}

	int Window::getMouseX() const
	{
		return static_cast<int>(m_mouse.x);
	}

	int Window::getMouseY() const
	{
		return static_cast<int>(m_mouse.y);
	}

	float Window::getMouseNormX() const
	{
		return ((float)m_mouse.x * 2.0f / (float)m_winSize.x) - 1.0f;
	}

	float Window::getMouseNormY() const
	{
		return ((float)m_mouse.y * -2.0f / (float)m_winSize.y) + 1.0f;
	}

	int Window::getMouseDX() const
	{
		return static_cast<int>(m_mouse.dx);
	}

	int Window::getMouseDY() const
	{
		return static_cast<int>(m_mouse.dy);
	}

	float Window::getMouseScrollX() const
	{
		return m_mouse.xscroll;
	}

	float Window::getMouseScrollY() const
	{
		return m_mouse.yscroll;
	}

	// TODO game pad

	// get timer value
	uint64_t Window::getNanos() const
	{
		uint64_t count;

		count = SDL_GetPerformanceCounter();
		if (m_counterFreq == BILLION) {
			return count;
		}
		else {
			return count * (BILLION / m_counterFreq);
		}
	}

	uint64_t Window::getLastFrameStamp() const
	{
		return m_lastFrameStamp;
	}

	uint64_t Window::getFrameCount() const
	{
		return m_frames;
	}

	uint64_t Window::getStartTime() const
	{
		return m_startTime;
	}

	float Window::dt() const
	{
		return (float)m_lastFrameTime / (float)BILLION;
	}

	uint64_t Window::getFPS() const
	{
		if (m_lastFrameTime == 0) return 0;
		return BILLION / m_lastFrameTime;
	}

	uint64_t Window::getAvgFPS() const
	{
		uint64_t delta_t = getNanos() - m_avgFpsStart;
		if (delta_t == 0) return 0;
		return BILLION * (m_frames - m_avgFpsStartCount) / delta_t;
	}

	void Window::resetAvgFPS()
	{
		m_avgFpsStart = getNanos();
		m_avgFpsStartCount = getFrameCount();
	}

	bool Window::infoBox(const std::string& title, const std::string& msg)
	{
		if (isFullscreen() == false) {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title.c_str(), msg.c_str(), m_handle);
			return true;
		}
		else {
			return false;
		}
	}

	/* STATIC METHODS */

	// Display an error message box
	void Window::errorBox(const std::string& message)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game Error", message.c_str(), NULL);
	}

}
