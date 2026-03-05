#pragma once

struct AppState;

namespace views {

enum class ViewId { Home, Help, Settings, Ticker, Error, Add };

bool handle_key_home(AppState& app, int ch);
bool handle_key_help(AppState& app, int ch);
bool handle_key_settings(AppState& app, int ch);
bool handle_key_ticker(AppState& app, int ch);
bool handle_key_error(AppState& app, int ch);
bool handle_key_add(AppState& app, int ch);

} // namespace views


