#ifndef ENGINE_TEST_SRC_GAME_H_
#define ENGINE_TEST_SRC_GAME_H_

struct GameSettings {
  bool enable_frame_limiter;
  bool enable_validation;
};

void PlayGame(GameSettings settings);

#endif