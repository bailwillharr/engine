#include <string.h>

#include <exception>
#include <unordered_set>

#include "logger.h"
#include "window.h"

#include "config.h"
#include "game.hpp"

int main(int argc, char* argv[]) {
  GameSettings settings{};
  settings.enable_frame_limiter = true;
  settings.enable_validation = false;
  if (argc >= 2) {
    std::unordered_set<std::string> args{};
    for (int i = 1; i < argc; i++) {
      args.insert(std::string(argv[i]));
    }
    if (args.contains("nofpslimit")) settings.enable_frame_limiter = false;
    if (args.contains("gpuvalidation")) settings.enable_validation = true;
  }

  engine::SetupLog(PROJECT_NAME);

  LOG_INFO("{} v{}", PROJECT_NAME, PROJECT_VERSION);

  try {
    PlayGame(settings);
  } catch (const std::exception& e) {
    LOG_CRITICAL("{}", e.what());
    engine::Window::ErrorBox(e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
