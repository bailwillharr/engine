#pragma once

namespace engine {

	struct AppInfo {
		const char* name;
		const char* version;
	};


	bool versionFromCharArray(const char* version, int* major, int* minor, int* patch);

}
