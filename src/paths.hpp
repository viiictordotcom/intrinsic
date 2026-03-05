#pragma once

#include <cstdlib>
#include <filesystem>
#include <string>

namespace intrinsic::platform {

inline bool env_path(const char* name, std::filesystem::path* out)
{
    if (!name || !out) return false;
    const char* value = std::getenv(name);
    if (!value || !*value) return false;
    *out = std::filesystem::path(value);
    return true;
}

inline std::filesystem::path config_home(std::string* err)
{
    namespace fs = std::filesystem;

    fs::path base;
    if (env_path("XDG_CONFIG_HOME", &base)) return base;

#if defined(__APPLE__)
    if (env_path("HOME", &base)) {
        return base / "Library" / "Application Support";
    }
    if (err) {
        *err = "Neither XDG_CONFIG_HOME nor HOME is set; cannot resolve "
               "config path";
    }
#else
    if (env_path("HOME", &base)) return base / ".config";
    if (err) {
        *err = "Neither XDG_CONFIG_HOME nor HOME is set; cannot resolve "
               "config path";
    }
#endif

    return {};
}

inline std::filesystem::path data_home(std::string* err)
{
    namespace fs = std::filesystem;

    fs::path base;
    if (env_path("XDG_DATA_HOME", &base)) return base;

#if defined(__APPLE__)
    if (env_path("HOME", &base)) {
        return base / "Library" / "Application Support";
    }
    if (err) {
        *err = "HOME is not set; cannot determine db location";
    }
#else
    if (env_path("HOME", &base)) return base / ".local" / "share";
    if (err) {
        *err = "HOME is not set; cannot determine db location";
    }
#endif

    return {};
}

} // namespace intrinsic::platform


