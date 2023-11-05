#include "window.h"

#include <iostream>
#include <stdexcept>

#include <imgui/imgui_impl_sdl2.h>

static const uint64_t BILLION = 1000000000;

namespace engine {

	Window::Window(const std::string& title, bool resizable, bool fullscreen)
		: title_(title), resizable_(resizable), fullscreen_(fullscreen)
	{

		// init SDL
		if (SDL_Init(SDL_INIT_VIDEO) != 0) {
			const std::string errMsg("Unable to initialise SDL: " + std::string(SDL_GetError()));
			throw std::runtime_error(errMsg);
		}

		counter_freq_ = SDL_GetPerformanceFrequency();
		start_time_ = GetNanos();
		last_frame_stamp_ = start_time_ - 1;
		avg_fps_start_ = start_time_;

		Uint32 windowFlags = SDL_WINDOW_SHOWN;

		// use Vulkan 1.3
		windowFlags |= SDL_WINDOW_VULKAN;

		if (resizable_) {
			windowFlags |= SDL_WINDOW_RESIZABLE;
		}

		if (fullscreen_) {
			windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		}

		// create the window
		handle_ = SDL_CreateWindow(
			title_.c_str(),
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			static_cast<int>(win_size_.x),
			static_cast<int>(win_size_.y),
			windowFlags);
		if (handle_ == NULL) {
			SDL_Quit();
			throw std::runtime_error("Unable to create window: " + std::string(SDL_GetError()));
		}

		const int WINDOWED_MIN_WIDTH = 640;
		const int WINDOWED_MIN_HEIGHT = 480;
		SDL_SetWindowMinimumSize(handle_, WINDOWED_MIN_WIDTH, WINDOWED_MIN_HEIGHT);

		// get window size
		int winWidth, winHeight;
		SDL_GetWindowSize(handle_, &winWidth, &winHeight);

		OnResize(winWidth, winHeight);

	}

	Window::~Window()
	{
		SDL_DestroyWindow(handle_);
		SDL_Quit();
	}

	// private methods

	void Window::OnResize(Sint32 width, Sint32 height)
	{
		// get window size
		win_size_.x = static_cast<int>(width);
		win_size_.y = static_cast<int>(height);

		just_resized_ = true;
	}

	void Window::ResetInputDeltas()
	{
		just_resized_ = false;

		keyboard_.deltas.fill(ButtonDelta::kSame);

		mouse_.deltas.fill(ButtonDelta::kSame);
		mouse_.dx = 0;
		mouse_.dy = 0;
		mouse_.xscroll = 0.0f;
		mouse_.yscroll = 0.0f;
	}

	// TODO event methods (like callbacks)

	void Window::OnWindowEvent(SDL_WindowEvent& e)
	{

		switch (e.event) {
		case SDL_WINDOWEVENT_SIZE_CHANGED:
			OnResize(e.data1, e.data2);
			break;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			keyboard_focus_ = true;
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			keyboard_focus_ = false;
			break;
		}
	}

	void Window::OnKeyEvent(SDL_KeyboardEvent& e)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureKeyboard) {
			keyboard_.deltas.fill(ButtonDelta::kSame);
		}
		else {
			bool keyWasDown = keyboard_.keys[e.keysym.scancode];
			bool keyIsDown = (e.state == SDL_PRESSED);
			keyboard_.keys[e.keysym.scancode] = keyIsDown;
			if (keyIsDown != keyWasDown) { // (if key was pressed or released)
				keyboard_.deltas[e.keysym.scancode] = keyIsDown ? ButtonDelta::kPressed : ButtonDelta::kReleased;
			}
		}
	}

	void Window::OnMouseButtonEvent(SDL_MouseButtonEvent& e)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse) {
			mouse_.deltas.fill(ButtonDelta::kSame);
		}
		else {
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
			bool buttonWasDown = mouse_.buttons.at(buttonIndex);
			bool buttonIsDown = (e.state == SDL_PRESSED);
			mouse_.buttons.at(buttonIndex) = buttonIsDown;
			if (buttonIsDown != buttonWasDown) { // (if button was pressed or released)
				// only sets delta if it hasn't already been set this frame (to detect very fast presses)
				if (mouse_.deltas[buttonIndex] == ButtonDelta::kSame) {
					mouse_.deltas[buttonIndex] = buttonIsDown ? ButtonDelta::kPressed : ButtonDelta::kReleased;
				}
			}
		}
	}

	void Window::OnMouseMotionEvent(SDL_MouseMotionEvent& e)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse) {
			mouse_.dx = 0;
			mouse_.dy = 0;
		}
		else {
			mouse_.x = e.x;
			mouse_.y = e.y;
			mouse_.dx = e.xrel;
			mouse_.dy = e.yrel;
		}
	}

	void Window::OnMouseWheelEvent(SDL_MouseWheelEvent& e)
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (!io.WantCaptureMouse) {
			if (e.direction == SDL_MOUSEWHEEL_NORMAL) {
				mouse_.xscroll = e.preciseX;
				mouse_.yscroll = e.preciseY;
			}
			else { // flipped
				mouse_.xscroll = -e.preciseX;
				mouse_.yscroll = -e.preciseY;
			}
		}
	}

	// public methods

	SDL_Window* Window::GetHandle() const
	{
		return handle_;
	}

	std::string Window::GetTitle() const
	{
		return title_;
	}

	void Window::GetInputAndEvents()
	{

		frames_++;
		uint64_t currentFrameStamp = GetNanos();
		last_frame_time_ = currentFrameStamp - last_frame_stamp_;
		last_frame_stamp_ = currentFrameStamp;

		ResetInputDeltas();

		// loop through all available events
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			ImGui_ImplSDL2_ProcessEvent(&e);
			switch (e.type) {

			case SDL_QUIT:
				SetCloseFlag();
				break;

			case SDL_WINDOWEVENT:
				OnWindowEvent(e.window);
				break;

			case SDL_KEYDOWN: // FALL THROUGH
			case SDL_KEYUP:
				OnKeyEvent(e.key);
				break;

			case SDL_MOUSEBUTTONDOWN: // FALL THROUGH
			case SDL_MOUSEBUTTONUP:
				OnMouseButtonEvent(e.button);
				break;

			case SDL_MOUSEMOTION:
				OnMouseMotionEvent(e.motion);
				break;

			case SDL_MOUSEWHEEL:
				OnMouseWheelEvent(e.wheel);
				break;

			}
		}

	}

	void Window::SetTitle(std::string title)
	{
		SDL_SetWindowTitle(handle_, title.c_str());
	}

	bool Window::GetWindowResized() const
	{
		return just_resized_;
	}

	void Window::SetResizedFlag()
	{
		just_resized_ = true;
	}

	void Window::Show()
	{
		SDL_ShowWindow(handle_);
	}

	void Window::Hide()
	{
		SDL_HideWindow(handle_);
	}

	void Window::Focus()
	{
		SDL_RaiseWindow(handle_);
		keyboard_focus_ = true;
	}

	bool Window::HasFocus() const
	{
		return keyboard_focus_;
	}

	void Window::SetCloseFlag()
	{
		should_close_ = true;
	}

	bool Window::IsRunning() const
	{
		return !should_close_;
	}

	void Window::SetFullscreen(bool fullscreen, bool exclusive)
	{

		if (resizable_) {

			SDL_DisplayMode mode;
			SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(handle_), &mode);
			SDL_SetWindowDisplayMode(handle_, &mode);

			if (SDL_SetWindowFullscreen(handle_, fullscreen ? (exclusive ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP) : 0) != 0) {
				throw std::runtime_error("Unable to set window to fullscreen/windowed");
			}
			fullscreen_ = fullscreen;
			if (fullscreen) {

				int width, height;
				SDL_GetWindowSize(handle_, &width, &height);
				OnResize(width, height);
			}
		}
	}

	void Window::ToggleFullscreen()
	{
		SetFullscreen(!fullscreen_, true);
	}

	bool Window::IsFullscreen() const
	{
		return fullscreen_;
	}

	bool Window::SetRelativeMouseMode(bool enabled)
	{
		mouse_.captured = enabled;
		int code = SDL_SetRelativeMouseMode(static_cast<SDL_bool>(enabled));
		if (code != 0) {
			throw std::runtime_error("Unable to set relative mouse mode");
		}
		else {
			return true;
		}
	}

	bool Window::MouseCaptured()
	{
		return mouse_.captured;
	}

	// getting input

	bool Window::GetKey(inputs::Key key) const
	{
		return keyboard_.keys[static_cast<int>(key)];
	}

	bool Window::GetKeyPress(inputs::Key key) const
	{
		return keyboard_.deltas[static_cast<int>(key)] == ButtonDelta::kPressed;
	}

	bool Window::GetKeyRelease(inputs::Key key) const
	{
		return keyboard_.deltas[static_cast<int>(key)] == ButtonDelta::kReleased;
	}

	// TODO mouse input

	bool Window::GetButton(inputs::MouseButton button) const
	{
		return mouse_.buttons[static_cast<int>(button)];
	}

	bool Window::GetButtonPress(inputs::MouseButton button) const
	{
		return mouse_.deltas[static_cast<int>(button)] == ButtonDelta::kPressed;
	}

	bool Window::GetButtonRelease(inputs::MouseButton button) const
	{
		return mouse_.deltas[static_cast<int>(button)] == ButtonDelta::kReleased;
	}

	int Window::GetMouseX() const
	{
		return static_cast<int>(mouse_.x);
	}

	int Window::GetMouseY() const
	{
		return static_cast<int>(mouse_.y);
	}

	float Window::GetMouseNormX() const
	{
		return ((float)mouse_.x * 2.0f / (float)win_size_.x) - 1.0f;
	}

	float Window::GetMouseNormY() const
	{
		return ((float)mouse_.y * -2.0f / (float)win_size_.y) + 1.0f;
	}

	int Window::GetMouseDX() const
	{
		return static_cast<int>(mouse_.dx);
	}

	int Window::GetMouseDY() const
	{
		return static_cast<int>(mouse_.dy);
	}

	float Window::GetMouseScrollX() const
	{
		return mouse_.xscroll;
	}

	float Window::GetMouseScrollY() const
	{
		return mouse_.yscroll;
	}

	// TODO game pad

	// get timer value
	uint64_t Window::GetNanos() const
	{
		uint64_t count;

		count = SDL_GetPerformanceCounter();
		if (counter_freq_ == BILLION) {
			return count;
		}
		else {
			return count * (BILLION / counter_freq_);
		}
	}

	uint64_t Window::GetLastFrameStamp() const
	{
		return last_frame_stamp_;
	}

	uint64_t Window::GetFrameCount() const
	{
		return frames_;
	}

	uint64_t Window::GetStartTime() const
	{
		return start_time_;
	}

	float Window::dt() const
	{
		return (float)last_frame_time_ / (float)BILLION;
	}

	uint64_t Window::GetFPS() const
	{
		if (last_frame_time_ == 0) return 0;
		return BILLION / last_frame_time_;
	}

	uint64_t Window::GetAvgFPS() const
	{
		uint64_t delta_t = GetNanos() - avg_fps_start_;
		if (delta_t == 0) return 0;
		return BILLION * (frames_ - avg_fps_start_count_) / delta_t;
	}

	void Window::ResetAvgFPS()
	{
		avg_fps_start_ = GetNanos();
		avg_fps_start_count_ = GetFrameCount();
	}

	bool Window::InfoBox(const std::string& title, const std::string& msg)
	{
		if (IsFullscreen() == false) {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, title.c_str(), msg.c_str(), handle_);
			return true;
		}
		else {
			return false;
		}
	}

	/* STATIC METHODS */

	// Display an error message box
	void Window::ErrorBox(const std::string& message)
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game Error", message.c_str(), NULL);
	}

}
