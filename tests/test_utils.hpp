#pragma once

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>

namespace test {

class ScopedEnvVar {
public:
    ScopedEnvVar(std::string name, std::optional<std::string> value)
        : name_(std::move(name))
    {
        const char* current = std::getenv(name_.c_str());
        if (current) {
            had_old_ = true;
            old_value_ = current;
        }

        apply(value);
    }

    ~ScopedEnvVar()
    {
        if (had_old_) {
            (void)set_env_(name_.c_str(), old_value_.c_str());
        }
        else {
            (void)unset_env_(name_.c_str());
        }
    }

    ScopedEnvVar(const ScopedEnvVar&) = delete;
    ScopedEnvVar& operator=(const ScopedEnvVar&) = delete;

private:
    static int set_env_(const char* name, const char* value)
    {
        return setenv(name, value, 1);
    }

    static int unset_env_(const char* name) { return unsetenv(name); }

    void apply(const std::optional<std::string>& value)
    {
        if (value.has_value()) {
            if (set_env_(name_.c_str(), value->c_str()) != 0) {
                throw std::runtime_error("setenv failed for " + name_);
            }
            return;
        }

        if (unset_env_(name_.c_str()) != 0) {
            throw std::runtime_error("unsetenv failed for " + name_);
        }
    }

private:
    std::string name_;
    std::string old_value_;
    bool had_old_{false};
};

class TempDir {
public:
    TempDir()
    {
        namespace fs = std::filesystem;

        const fs::path base = fs::temp_directory_path() / "intrinsic-tests";
        std::error_code ec;
        fs::create_directories(base, ec);
        if (ec) {
            throw std::runtime_error("failed to create test base directory: " +
                                     ec.message());
        }

        path_ = base / unique_name_();
        fs::create_directories(path_, ec);
        if (ec) {
            throw std::runtime_error("failed to create test temp directory: " +
                                     ec.message());
        }
    }

    ~TempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return path_; }

private:
    static std::string unique_name_()
    {
        static std::atomic<std::uint64_t> counter{0};
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        const auto tick =
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

        return std::string("run-") + std::to_string(tick) + "-" +
               std::to_string(counter.fetch_add(1));
    }

private:
    std::filesystem::path path_;
};

inline void write_text_file(const std::filesystem::path& path,
                            const std::string& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to open file for write: " +
                                 path.string());
    }

    out << text;
    out.close();
    if (!out) {
        throw std::runtime_error("failed to write file: " + path.string());
    }
}

} // namespace test


