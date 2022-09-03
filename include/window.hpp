#pragma once

#include "export.h"

#include "inputs/keyboard.hpp"
#include "inputs/mouse.hpp"

#pragma warning (push, 0)
#include <SDL.h>
#pragma warning (pop)

#include <glm/vec2.hpp>

#include <array>
#include <string>

ENGINE_API extern const uint64_t BILLION;

class ENGINE_API Window {

public:
	Window(const std::string& title);
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;
	~Window();

	// Return the title name
	std::string getTitle() const;

	// Make this window the current OpenGL context.
	// This is already done in window initialisation.
	void makeContextCurrent();

	// Tell the GPU to render the back buffer to the screen.
	// Run this on every frame.
	void swapBuffers();
	// Update the window state to capture any events that have occurred.
	// Run this on every frame.
	void getInputAndEvents();

	// if 'true', swapBuffers() will wait in order to synchronise with the
	// monitor's refresh rate.
	void setVSync(bool enable);
	// Returns true if VSync is enabled.
	bool getVSync() const;

	glm::ivec2 getViewportSize();

	void setTitle(std::string title);

	// Hides the window (it will appear closed to the user).
	void hide();
	// Shows the window again.
	void show();
	// Raises the window above other windows and sets the input focus
	void focus();
	// Returns true if the window has focus
	bool hasFocus() const;

	// Sets the close flag, check this with shouldClose()
	void setCloseFlag();
	// Returns true if the window should remain open
	bool isRunning() const;

	void setFullscreen(bool fullscreen, bool exclusive=true);
	void toggleFullscreen();

	bool isFullscreen() const;

	// Relative mouse mode captures the cursor for FPS style use. Returns false if unsupported.
	bool setRelativeMouseMode(bool enabled);

	// returns true if relative mouse mode is enabled
	bool mouseCaptured();

	// window events

	// Returns true if the window was just resized during the previous frame
	bool getWindowResized() const;
	// Set the window resized flag (to recalculate aspect ratios and such)
	inline void setResizedFlag()
	{
		m_justResized = true;
	}

	// keyboard events

	// returns true if key is down
	bool getKey(inputs::Key key) const;
	// returns true if key was just pressed
	bool getKeyPress(inputs::Key key) const;
	// returns true if key was just released
	bool getKeyRelease(inputs::Key key) const;

	// mouse events

	// returns true if button is down
	bool getButton(inputs::MouseButton button) const;
	// returns true if button was just pressed
	bool getButtonPress(inputs::MouseButton button) const;
	// returns true if button was just released
	bool getButtonRelease(inputs::MouseButton button) const;

	// retrieves x coordinate of the mouse
	int getMouseX() const;
	// retrieves y coordinate of the mouse
	int getMouseY() const;
	// retrieves mouse x coordinate normalised for OpenGL
	float getMouseNormX() const;
	// retrieves mouse y coordinate normalised for OpenGL
	float getMouseNormY() const;
	// retrieves dx of the mouse since the last frame
	int getMouseDX() const;
	// retrieves dy of the mouse since the last frame
	int getMouseDY() const;
	// retrieves amount scrolled vertically
	float getMouseScrollX() const;
	// retrieves amount scrolled horizontally
	float getMouseScrollY() const;

	// joystick/gamepad events (maybe), other misc events



	// returns the performance counter value in nanoseconds;
	uint64_t getNanos() const;
	// get the time recorded at the end of the last frame
	uint64_t getLastFrameStamp() const;

	// returns the number of frames elapsed since window creation
	uint64_t getFrameCount() const;
	uint64_t getStartTime() const;;
	float dt() const; // returns delta time in seconds
	uint64_t getFPS() const;
	uint64_t getAvgFPS() const;

	void resetAvgFPS();

	bool infoBox(const std::string& title, const std::string& msg);

	/* STATIC METHODS */
	static void errorBox(const std::string& message);

	private:
		SDL_Window* m_handle;
		SDL_GLContext m_glContext;

		bool m_shouldClose = false;

		std::string m_title;

		bool m_fullscreen = false;
		bool m_justResized = false;
		bool m_keyboardFocus = true;

		// size in screen coordinates
		glm::ivec2 m_winSize = glm::vec2(640, 480);
		// actual framebuffer size
		glm::ivec2 m_fbSize;

		// performance counter frequency
		uint64_t m_counterFreq;

		// number of frames swapped
		uint64_t m_frames = 0;
		// frame count offset for fpsAvg
		uint64_t m_avgFpsStartCount = 0;
		// in nanoseconds
		uint64_t m_startTime;
		// in nanoseconds
		uint64_t m_lastFrameStamp;
		// in nanoseconds; elapsed time between frames
		uint64_t m_lastFrameTime = 1; // not 0 to avoid division by zero
		// in nanoseconds
		uint64_t m_avgFpsStart;

		// input stuff

		enum class ButtonDelta {
			SAME = 0,
			PRESSED,
			RELEASED
		};

		struct {
			std::array<bool, SDL_NUM_SCANCODES> keys;
			std::array<enum ButtonDelta, SDL_NUM_SCANCODES> deltas;
		} m_keyboard{ };

		struct {
			std::array<bool, static_cast<int>(inputs::MouseButton::M_SIZE)> buttons;
			std::array<enum ButtonDelta, 8> deltas;
			Sint32 x;
			Sint32 y;
			Sint32 dx;
			Sint32 dy;
			float xscroll;
			float yscroll;
			bool captured = false;
		} m_mouse{ };

		// private methods

		void onResize(Sint32 width, Sint32 height);
		void resetInputDeltas();

		// event methods (like callbacks)

		void onWindowEvent(SDL_WindowEvent& e);
		void onKeyEvent(SDL_KeyboardEvent& e);
		void onMouseButtonEvent(SDL_MouseButtonEvent& e);
		void onMouseMotionEvent(SDL_MouseMotionEvent& e);
		void onMouseWheelEvent(SDL_MouseWheelEvent& e);

};
