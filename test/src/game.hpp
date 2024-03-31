#pragma once

struct GameSettings {
    bool enable_frame_limiter;
    bool enable_validation;
};

void PlayGame(GameSettings settings);