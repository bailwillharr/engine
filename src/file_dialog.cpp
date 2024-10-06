#include "file_dialog.h"

#ifdef _WIN32
#include <windows.h>
#define WIN_MAX_PATH 260
#endif

#include <filesystem>
#include <string>
#include <vector>

#include "log.h"

namespace engine {

std::filesystem::path openFileDialog(const std::vector<std::string>& extensions)
{
#ifdef _WIN32

    // build the filter string
    std::string wildcards{};
    for (const std::string& ext : extensions) {
        wildcards += "*." + ext + ";";
    }
    wildcards.pop_back(); // remove the last semicolon
    const std::string filter = "(" + wildcards + ")\0" + wildcards + "\0All Files (*.*)\0*.*\0";

    OPENFILENAMEA ofn{};             // common dialog box structure
    CHAR szFile[WIN_MAX_PATH] = {0}; // if using TCHAR macros, use TCHAR array

    // Initialize OPENFILENAME
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter.c_str();
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    // Display the Open dialog box
    if (GetOpenFileNameA(&ofn) == TRUE) {
        return std::filesystem::path(std::string(ofn.lpstrFile));
    }
    else {
        return std::filesystem::path{}; // User cancelled the dialog
    }
#else
    // only Windows dialogs supported at the moment
    (void)extensions;
    LOG_ERROR("Open file dialog not supported on this platform");
    return "";
#endif
}

} // namespace engine
