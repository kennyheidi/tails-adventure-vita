#ifndef TA_FILESYSTEM_H
#define TA_FILESYSTEM_H

#include <string>

#ifdef __vita__
// VitaSDK ships a partial std::filesystem — include it explicitly
#include <filesystem>
#else
#include <filesystem>
#endif

namespace TA::filesystem {
    bool fileExists(std::filesystem::path path);
    std::string readFile(std::filesystem::path path);
    std::string readAsset(std::filesystem::path path);
    std::filesystem::path getAssetsPath();
    std::filesystem::path getExecutableDirectory();
    void writeFile(std::filesystem::path path, std::string value);
}

#endif // TA_FILESYSTEM_H
