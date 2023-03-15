#pragma once

struct GameSettings {
	bool enableFrameLimiter;
	bool enableValidation;
};

void playGame(GameSettings settings);
