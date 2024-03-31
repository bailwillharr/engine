#pragma once

#include <array>
#include <string>

#include <SDL.h>
#include <glm/vec2.hpp>

#include "inputs/keyboard.h"
#include "inputs/mouse.h"

namespace engine {

class Window {
   public:
    Window(const std::string& title, bool resizable = true, bool fullscreen = true);
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    ~Window();

    SDL_Window* GetHandle() const;

    // Return the title name
    std::string GetTitle() const;

    // Update the window state to capture any events that have occurred.
    // Run this on every frame.
    void GetInputAndEvents();

    void SetTitle(std::string title);

    // Hides the window (it will appear closed to the user).
    void Hide();
    // Shows the window again.
    void Show();
    // Raises the window above other windows and sets the input focus
    void Focus();
    // Returns true if the window has focus
    bool HasFocus() const;

    // Sets the close flag, check this with shouldClose()
    void SetCloseFlag();
    // Returns true if the window should remain open
    bool IsRunning() const;

    void SetFullscreen(bool fullscreen, bool exclusive = false);
    void ToggleFullscreen();

    bool IsFullscreen() const;

    // Relative mouse mode captures the cursor for FPS style use.
    // Returns false if unsupported.
    bool SetRelativeMouseMode(bool enabled);

    // returns true if relative mouse mode is enabled
    bool MouseCaptured();

    // window events

    // Returns true if the window was just resized during the previous frame
    bool GetWindowResized() const;
    // Set the window resized flag (to recalculate aspect ratios and such)
    void SetResizedFlag();

    // keyboard events

    // returns true if key is down
    bool GetKey(inputs::Key key) const;
    // returns true if key was just pressed
    bool GetKeyPress(inputs::Key key) const;
    // returns true if key was just released
    bool GetKeyRelease(inputs::Key key) const;

    // mouse events

    // returns true if button is down
    bool GetButton(inputs::MouseButton button) const;
    // returns true if button was just pressed
    bool GetButtonPress(inputs::MouseButton button) const;
    // returns true if button was just released
    bool GetButtonRelease(inputs::MouseButton button) const;

    // retrieves x coordinate of the mouse
    int GetMouseX() const;
    // retrieves y coordinate of the mouse
    int GetMouseY() const;
    // retrieves mouse x coordinate normalised for OpenGL
    float GetMouseNormX() const;
    // retrieves mouse y coordinate normalised for OpenGL
    float GetMouseNormY() const;
    // retrieves dx of the mouse since the last frame
    int GetMouseDX() const;
    // retrieves dy of the mouse since the last frame
    int GetMouseDY() const;
    // retrieves amount scrolled vertically
    float GetMouseScrollX() const;
    // retrieves amount scrolled horizontally
    float GetMouseScrollY() const;

    // joystick/gamepad events (maybe), other misc events

    // returns the performance counter value in nanoseconds;
    uint64_t GetNanos() const;
    // get the time recorded at the end of the last frame
    uint64_t GetLastFrameStamp() const;

    // returns the number of frames elapsed since window creation
    uint64_t GetFrameCount() const;
    uint64_t GetStartTime() const;
    float dt() const; // returns delta time in seconds
    uint64_t GetFPS() const;
    uint64_t GetAvgFPS() const;

    void ResetAvgFPS();

    bool InfoBox(const std::string& title, const std::string& msg);

    /* STATIC METHODS */
    static void ErrorBox(const std::string& message);

   private:
    SDL_Window* handle_;

    bool should_close_ = false;

    std::string title_;

    bool resizable_;

    bool fullscreen_ = false;
    bool just_resized_ = false;
    bool keyboard_focus_ = true;

    // size in screen coordinates
    glm::ivec2 win_size_ = glm::vec2(1024, 768);

    // performance counter frequency
    uint64_t counter_freq_;

    // number of frames swapped
    uint64_t frames_ = 0;
    // frame count offset for fpsAvg
    uint64_t avg_fps_start_count_ = 0;
    // in nanoseconds
    uint64_t start_time_;
    // in nanoseconds
    uint64_t last_frame_stamp_;
    // in nanoseconds; elapsed time between frames
    uint64_t last_frame_time_ = 1; // not 0 to avoid division by zero
    // in nanoseconds
    uint64_t avg_fps_start_;

    // input stuff

    enum class ButtonDelta { kSame = 0, kPressed, kReleased };

    struct {
        std::array<bool, SDL_NUM_SCANCODES> keys;
        std::array<enum ButtonDelta, SDL_NUM_SCANCODES> deltas;
    } keyboard_{};

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
    } mouse_{};

    // private methods

    void OnResize(Sint32 width, Sint32 height);
    void ResetInputDeltas();

    // event methods (like callbacks)

    void OnWindowEvent(SDL_WindowEvent& e);
    void OnKeyEvent(SDL_KeyboardEvent& e);
    void OnMouseButtonEvent(SDL_MouseButtonEvent& e);
    void OnMouseMotionEvent(SDL_MouseMotionEvent& e);
    void OnMouseWheelEvent(SDL_MouseWheelEvent& e);
};

} // namespace engine