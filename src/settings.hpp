#pragma once
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "paths.hpp"
#include "state.hpp"

inline std::string trim_copy(std::string s)
{
    auto not_space = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
    s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
    return s;
}

inline std::string lower_copy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

inline std::filesystem::path intrinsic_config_path(std::string* err)
{
    try {
        const auto base = intrinsic::platform::config_home(err);
        if (base.empty()) return {};
        return base / "intrinsic" / "config.ini";
    }
    catch (const std::exception& e) {
        if (err) *err = e.what();
        return {};
    }
}

inline bool save_settings(const AppState::Settings& s, std::string* err)
{
    try {
        namespace fs = std::filesystem;

        std::string path_err;
        fs::path cfg = intrinsic_config_path(&path_err);
        if (cfg.empty()) {
            if (err) *err = path_err;
            return false;
        }

        fs::create_directories(cfg.parent_path());

        const fs::path tmp = cfg.string() + ".tmp";

        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            if (err)
                *err = "failed to open config for writing: " + tmp.string();
            return false;
        }

        auto sort_key_str =
            (s.sort_key == db::Database::TickerSortKey::LastUpdate)
                ? "last_update"
                : "ticker";
        auto sort_dir_str =
            (s.sort_dir == db::Database::SortDir::Desc) ? "desc" : "asc";

        out << "sort_key=" << sort_key_str << "\n";
        out << "sort_dir=" << sort_dir_str << "\n";
        out << "ttm=" << (s.ttm ? "1" : "0") << "\n";
        out << "show_help=" << (s.show_help ? "1" : "0") << "\n";
        out.flush();
        out.close();

        // replace atomically -> best-effort across platforms
        std::error_code ec;
        fs::rename(tmp, cfg, ec);
        if (ec) {
            fs::remove(cfg, ec); // ignore
            ec.clear();
            fs::rename(tmp, cfg, ec);
        }
        if (ec) {
            if (err) *err = "failed to write config: " + ec.message();
            return false;
        }

        return true;
    }
    catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    }
}

inline bool load_settings(AppState::Settings& s, std::string* err)
{
    try {
        namespace fs = std::filesystem;

        std::string path_err;
        fs::path cfg = intrinsic_config_path(&path_err);
        if (cfg.empty()) {
            if (err) *err = path_err;
            return false;
        }

        std::ifstream in(cfg, std::ios::binary);
        if (!in) {
            // no file yet -> defaults remain
            return true;
        }

        std::string line;
        while (std::getline(in, line)) {
            line = trim_copy(line);
            if (line.empty()) continue;
            if (line[0] == '#' || line[0] == ';') continue;

            const auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = lower_copy(trim_copy(line.substr(0, eq)));
            std::string val = lower_copy(trim_copy(line.substr(eq + 1)));

            if (key == "sort_key") {
                if (val == "last_update" || val == "lastupdate") {
                    s.sort_key = db::Database::TickerSortKey::LastUpdate;
                }
                else if (val == "ticker") {
                    s.sort_key = db::Database::TickerSortKey::Ticker;
                }
            }
            else if (key == "sort_dir" || key == "sort_order") {
                if (val == "desc" || val == "d") {
                    s.sort_dir = db::Database::SortDir::Desc;
                }
                else if (val == "asc" || val == "a") {
                    s.sort_dir = db::Database::SortDir::Asc;
                }
            }
            else if (key == "ttm") {
                if (val == "1" || val == "true" || val == "yes" || val == "on")
                    s.ttm = true;
                if (val == "0" || val == "false" || val == "no" || val == "off")
                    s.ttm = false;
            }
            else if (key == "show_help" || key == "help" || key == "hints") {
                if (val == "1" || val == "true" || val == "yes" || val == "on")
                    s.show_help = true;
                if (val == "0" || val == "false" || val == "no" || val == "off")
                    s.show_help = false;
            }
        }

        return true;
    }
    catch (const std::exception& e) {
        if (err) *err = e.what();
        return false;
    }
}

