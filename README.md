# Multi-Asset Market Making Engine

A C++20 multi-asset market making engine that quotes two-sided markets across multiple instruments and venues simultaneously. The engine manages inventory risk, dynamic spread, and quote skew, and includes a full backtest/simulation harness.

## Architecture

```
┌─────────────────────┐     ┌──────────────┐     ┌───────────────────┐
│ MarketDataAggregator│────▶│  QuoteEngine │────▶│  VenueRouter      │
│ (mid, spread, vol)  │     │ (bid/ask/size)│     │ (fee/latency opt) │
└─────────────────────┘     └──────┬───────┘     └────────┬──────────┘
                                   │                      │
                            ┌──────▼──────────────────────▼──────────┐
                            │       MarketMakerController            │
                            │       (event loop / orchestrator)      │
                            └──────┬─────────────────────┬───────────┘
                                   │                     │
                            ┌──────▼──────┐       ┌──────▼──────────┐
                            │ RiskManager │       │ExecutionGateway │
                            │ (pos/P&L)   │       │(Sim / Null)     │
                            └─────────────┘       └─────────────────┘
```

**Components:**

| Component | Responsibility |
|---|---|
| **MarketDataAggregator** | Builds per-instrument market views from raw venue book snapshots. Computes mid price, spread, EWMA volatility, and weighted depth across venues. |
| **RiskManager** | Tracks positions, realized/unrealized P&L, and enforces per-instrument position limits. |
| **QuoteEngine** | Computes bid/ask prices and sizes using dynamic spread (volatility-adjusted), inventory skew, and position-aware sizing. |
| **VenueRouter** | Selects the optimal venue per quote based on maker fees, latency, cancel penalty, and book depth. |
| **MarketMakerController** | Event-driven controller that wires everything together — on each market data update, it re-quotes eligible instruments. |
| **IExecutionGateway** | Abstract interface for order management. `SimExecutionGateway` simulates fills against the book; `NullExecutionGateway` is a dry-run stub. |
| **BacktestRunner** | Feeds historical CSV or synthetic random-walk data through the full pipeline and collects metrics. |

## Quoting Strategy

The engine implements a baseline multi-asset market maker with:

- **Dynamic spread**: `spread = clamp(base + α_σ · σ, min, max)` — widens in high volatility
- **Inventory skew**: `Δ = α_q · (q / Q_max) · spread` — shifts quotes to encourage rebalancing
- **Position-aware sizing**: `size = s₀ · (1 − β · |q̃|)` — shrinks size as inventory approaches limits

When long, both bid and ask shift down (sell more aggressively). When short, the opposite.

## Project Structure

```
market_making_engine/
├── CMakeLists.txt
├── include/
│   ├── config/          # InstrumentConfig, VenueConfig
│   ├── market/          # MarketView, MarketDataAggregator
│   ├── risk/            # Portfolio, RiskManager
│   ├── strategy/        # MarketMakingParams, QuoteEngine, MarketMakerController
│   ├── execution/       # IExecutionGateway, SimExecutionGateway, VenueRouter
│   └── backtest/        # BacktestRunner, Metrics
├── src/                 # Implementation files
├── tests/
│   ├── unit/            # 34 unit tests (all components)
│   └── integration/     # 6 end-to-end tests
└── data/
    ├── config.json      # 5 instruments, 2 venues
    └── sample_lob_data.csv
```

## Building

Requires CMake 3.16+ and a C++20 compiler. GoogleTest is fetched automatically.

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

## Running Tests

```bash
cd build
ctest --output-on-failure
```

Or individually:

```bash
./unit_tests          # 34 unit tests
./integration_tests   # 6 integration tests
```

## Running the Engine

**Synthetic backtest** (default — random-walk LOB data):

```bash
./build/market_maker --config data/config.json --ticks 10000
```

**Historical data backtest**:

```bash
./build/market_maker --config data/config.json --data
```

**Options:**

| Flag | Description |
|---|---|
| `--config <path>` | Path to JSON config file (default: `data/config.json`) |
| `--ticks <n>` | Number of synthetic ticks (default: 10000) |
| `--data` | Use CSV data file from config instead of synthetic data |
| `--help` | Show usage |

**Output:**

- `REPORT.md` — per-instrument and global metrics (P&L, Sharpe, max drawdown, spread captured, fill counts)
- `data/backtest_results.csv` — tick-by-tick time series

## Configuration

`data/config.json` defines instruments, venues, and per-instrument strategy parameters:

```json
{
    "instruments": [
        {
            "id": 1,
            "symbol": "AAPL",
            "tick_size": 0.01,
            "lot_size": 1.0,
            "inventory_limit": 100.0,
            "params": {
                "base_spread_bp": 10.0,
                "min_spread_bp": 2.0,
                "max_spread_bp": 50.0,
                "volatility_coeff": 1.5,
                "inventory_coeff": 0.5,
                "size_base": 5.0,
                "max_position": 100.0
            }
        }
    ],
    "venues": [
        {
            "id": 1,
            "name": "NYSE",
            "maker_fee_bp": 0.5,
            "taker_fee_bp": 1.5,
            "latency_ms": 0.5
        }
    ]
}
```

The default config ships with 5 instruments (AAPL, MSFT, GOOGL, AMZN, TSLA) and 2 venues (NYSE, NASDAQ).

## Extending

The engine is designed for later integration with real venues:

- Implement `IExecutionGateway` to talk to a broker/exchange API
- The strategy logic (`QuoteEngine`, `RiskManager`) is fully decoupled from execution infrastructure
- `VenueRouter` can be extended with more sophisticated routing (e.g. latency-aware, fill-rate weighted)
