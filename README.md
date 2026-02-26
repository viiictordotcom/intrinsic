# intrinsic

Analyze stocks from the terminal. Track your companies with a fast ncurses + SQLite workflow. Distributed as a **Nix-only** package.

<p align="center">
  <img src="intrinsic_img.png" alt="Intrinsic terminal app" width="400"/>
</p>

The name intrinsic comes from Benjamin Graham’s famous concept of intrinsic value. The true worth of a company based on its fundamentals, rather than market speculation. The app is designed to bring this philosophy into your workflow, giving you a clear, data-driven view of the businesses you’re evaluating.

## Main dependencies

- C++20
- ncurses
- SQLite
- CMake + Ninja
- Nix (installation, build, update)

## Supported platforms

- Linux: `x86_64`, `aarch64`
- macOS: `x86_64`, `aarch64`
- Windows (WSL2): use Linux package/install flow inside WSL (`x86_64`, `aarch64`).

WSL notes:
- Run install/update commands from a WSL shell, not native PowerShell/cmd.
- App data/settings are persisted in the WSL Linux filesystem (same Linux paths as above).
- Clipboard copy (`c`) requires one of: `wl-copy`, `xclip`, or `xsel` to be available in WSL.

## Install and compile

- Install to your user profile (enables the `intrinsic` command):
```bash
nix profile add github:viiictordotcom/intrinsic#default
```

How it works:
- `nix profile add` builds (if needed) and installs the app in your user profile.
- Build artifacts are handled by Nix in the store; no manual build script is required.

## Update

Rolling channel only (no version pinning):
- `nix profile upgrade intrinsic --refresh` checks upstream and updates to latest available revision.

Optional in-app update:
1. Open `Settings`.
2. Press `U`, then press `U` again to execute update.
3. Restart `intrinsic` after update.

## Typical flow

1. Install once:
```bash
nix profile add github:viiictordotcom/intrinsic#default
```
2. Launch:
```bash
intrinsic
```
3. Update later:
```bash
nix profile upgrade intrinsic --refresh
```
Or in-app: open `Settings`, press `U`, then press `U` again.

## Usage and key bindings

Global:
- `q`: quit
- `h`: home
- `?`: help
- `s`: settings

Home view:
- `a`: add record
- `p`: mark/unmark selected ticker as a portfolio ticker
- `P`: toggle portfolio-only view (and portfolio-scoped search)
- `space`: search mode
- `esc`: exit search
- `arrow keys`: move selection / page navigation
- `enter`: open selected ticker

Ticker view:
- `left/right`: previous/next period
- `up/down`: switch input field (`price`, `wished per`)
- `PageUp/PageDown`: scroll metrics
- `y`: toggle yearly-only view
- `e`: edit selected period
- `x`: delete selected period
- `c`: copy period + derived metrics to clipboard
- `Backspace/Delete`: edit active input
- `esc`: back to home

Add/Edit view:
- `arrow keys`: move field/cursor
- `tab`: switch add form type (`t1` / `t2`) for new tickers
- `enter`: validate and open confirm prompt
- `y` / `n`: confirm or cancel write
- `esc`: cancel and return

Settings view:
- `H`: toggle help hints
- `S`: toggle ticker sort key
- `O`: toggle sort direction
- `T`: toggle TTM mode
- `U`: update (double-press confirmation)
- `N`: nuke/reset data + settings (double-press confirmation)

## Inputs

### Ticker types

- `type 1` (default): current/general company structure (balance + income + cash flow).
- `type 2`: bank structure (loans/deposits/regulatory/asset quality).
- Ticker type is stored per ticker and cannot be mixed across periods.
- In `Add` view, `Tab` switches type only for new tickers; existing tickers auto-lock to their saved type.

Ticker field:
- Normalized to uppercase
- Allowed chars: `A-Z`, `0-9`, `.`
- Max length: 12
- Consecutive dots are collapsed

Period field:
- Format: `YYYY-TYPE`
- Allowed types: `Y`, `Q1`, `Q2`, `Q3`, `Q4`, `S1`, `S2`

Examples:
- `2024-Y`
- `2025-Q3`
- `2023-S1`

## Metrics

Metrics are shown for the selected period in Ticker view.

### TTM mode (`Settings` -> `T`)

- `TTM` means trailing twelve months.
- For `Q*` periods, TTM uses the sum of the latest 4 quarterly values.
- For `S*` periods, TTM uses the sum of the latest 2 semiannual values.
- For `Y` periods, TTM does not apply (yearly values are already full-year).
- TTM is applied to `EPS`, `Net Income`, and `CFop` inputs used by derived metrics.
- With `TTM on`, valuation/derived metrics are smoothed when enough valid history exists.
- With `TTM off`, metrics use only the selected period values.
- With `TTM off` on quarterly/semiannual data, metrics can look more volatile or seasonally distorted (for example temporarily higher/lower `P / E`, `EV / NI`, and `EV / CFop`).
- If TTM is on but the required history is incomplete/invalid, the app falls back to the selected period values.

### Change values (`... %`)

- Most metrics show a change suffix vs the same period in the previous year (`YYYY-<same type>`).
- If the previous same-period row is missing, or a needed denominator is zero/invalid, the change suffix is omitted.
- Absolute-value metrics use: `((current - previous) / abs(previous)) * 100`.
- Ratio metrics use sign-aware rules:
    - If both values are negative, an improvement is shown when absolute value gets smaller.
    - If previous is negative and current is positive, change is shown as a positive crossover.
    - Otherwise, standard ratio delta is used: `((current - previous) / previous) * 100`.
- For valuation multiples where lower is better (`P / E`, `P / BV`, `P / TBV`, `EVcap`, `EV / CFop`, `EV / NI`), change coloring is inverted in UI.
- `P needed` and `NI needed` are not YoY metrics:
- `P needed` change is relative to the typed `price`.
- `NI needed` change is relative to current/TTM net income baseline.

### Input fields

- `price`: typed share price used by price-dependent metrics.
- `wished per`: target P/E multiple used for target calculations.
- Input format for both fields: digits plus one decimal point, max length 16.
- If `price` is empty/zero/invalid, price-dependent metrics render as `--`.

### Input-related metrics

- `P needed`: `round(wished per * eps_used)`. In TTM mode, `eps_used` is TTM EPS when available for `Q*`/`S*`, otherwise period EPS.
- `NI needed`: `required_eps * shares_used`, where `required_eps = price / wished per` and `shares_used = ni_used / eps_used` (TTM-aware when enabled).

### Data

<details>
<summary><strong>Type 1</strong></summary>

- `P / E`: price-to-earnings
- `P / BV`: price-to-book value
- `EV`: enterprise value
- `EVcap`: `EV / market cap`
- `EV / CFop`: `EV / cash flow from operations`
- `EV / NI`: `EV / net income`
- `P needed`: price implied by `wished per` and EPS
- `NI needed`: net income needed for current `price` at `wished per`
- `CA`: current assets
- `Cash`: cash and equivalents
- `NCA`: non-current assets
- `TA`: total assets (`CA + NCA`)
- `CL`: current liabilities
- `NCL`: non-current liabilities
- `TL`: total liabilities (`CL + NCL`)
- `E`: equity (`TA - TL`)
- `WC`: working capital (`CA - CL`)
- `WC / NCL`: working capital to non-current liabilities
- `Shs~`: approximate shares (`net income / EPS`, rounded)
- `BV`: book value per share (`equity / Shs~`)
- `Liq.`: liquidity (`CA / CL`)
- `Sol.`: solvency (`TA / TL`)
- `Lev.`: leverage (`TL / E`)
- `CFop`: cash flow from operations
- `CFinv`: cash flow from investing
- `CFfin`: cash flow from financing
- `R`: revenue
- `NI`: net income
- `EPS`: earnings per share
- `Mnet`: net margin (`NI / R`)
- `ROA`: return on assets (`NI / TA`)
- `ROE`: return on equity (`NI / E`)

</details>

<details>
<summary><strong>Type 2</strong></summary>

- `P / E`: price-to-earnings
- `P / TBV`: price-to-tangible-book-value
- `TBV`: tangible book value per share (derived from `TE` and internal approximate shares)
- `P needed`: price implied by `wished per` and EPS
- `NI needed`: net income needed for current `price` at `wished per`
- `TA`: total assets
- `TL`: total liabilities
- `Loans`: total loans
- `Deposits`: total deposits
- `Goodwill`: goodwill
- `Loans / Dep.`: loan-to-deposit ratio
- `E`: equity (`TA - TL`)
- `TE`: tangible equity (`E - Goodwill`)
- `Lev.`: leverage (`TA / TE`)
- `NII`: net interest income
- `NonII`: non-interest income
- `NIExp`: non-interest expense
- `PPOP`: pre-provision profit (`NII + NonII - NIExp`)
- `LLP`: loan loss provisions
- `Prov / PPOP`: `LLP / PPOP`
- `NI`: net income
- `EPS`: earnings per share
- `ROA`: `NI / TA`
- `ROTE`: `NI / TE`
- `PPOP / A`: `PPOP / TA`
- `RWA`: risk-weighted assets
- `CET1`: common equity tier 1 capital
- `NPL`: non-performing loans
- `NCO`: net charge-offs
- `NPL%`: `NPL / Loans`
- `NCO%`: `NCO / Loans`
- `Prov%`: `LLP / Loans`
- `CET1%`: `CET1 / RWA`

</details>
