<img src="https://r2cdn.perplexity.ai/pplx-full-logo-primary-dark%402x.png" style="height:64px;margin-right:32px"/>

# Now I choose to work on project 2. Now prepare me plan and instructions that I can provide to the Claude code possibly in the md file which Claude code can grab and start working on this project.

Below is a ready-to-use `MARKET_MAKING_ENGINE_SPEC.md` you can drop into your repo and feed to Claude Code. It’s written so a coding assistant can implement it step by step.

***

# Multi‑Asset Market Making Engine – Project Specification

## 1. Project Overview

Build a **multi‑asset market making engine** in C++20 on top of an existing low‑latency matching/order‑book core. The engine should:

- Quote two‑sided markets (bid/ask) for many instruments simultaneously.
- Manage **inventory risk**, **spread**, and **quote skew** dynamically.
- Support **multiple venues** with different fees/latencies and simple routing rules.
- Run in both **simulation** (internal venue) and **plug‑in** mode (later integration with external APIs).

This project focuses on *strategy + risk + multi‑asset logic* rather than rebuilding the matching engine itself.[^1][^2][^3]

***

## 2. High‑Level Architecture

We assume an existing low‑latency “exchange” or matching engine component (from Project 1 or similar). On top of that, build:

1. **Market Data View**
    - Per‑instrument order book snapshot and derived metrics (mid, spread, depth, volatility).
2. **Risk \& Inventory Manager**
    - Tracks positions, P\&L, exposure per instrument and globally.
    - Enforces position and exposure limits.
3. **Quote Engine**
    - Computes bid/ask prices and sizes per instrument.
    - Adjusts quotes based on volatility, inventory, and venue characteristics.[^4][^5][^3]
4. **Venue Router**
    - For each instrument/quote, chooses which venue(s) to quote on.
    - Encapsulates venue fees and latency profiles.
5. **Execution Gateway Interface**
    - Abstract interface for sending/cancelling orders to:
        - Internal simulator.
        - (Later) external venues.
6. **Backtest / Simulation Harness**
    - Feeds historical or synthetic data.
    - Collects metrics on P\&L, spread capture, inventory distribution, and quote hit ratio.[^6][^7][^8]

***

## 3. Core Concepts \& Data Structures

### 3.1. Instrument \& Venue IDs

```cpp
using InstrumentId = uint32_t;
using VenueId      = uint8_t;

struct InstrumentConfig {
    InstrumentId id;
    std::string  symbol;
    double       tick_size;
    double       lot_size;
    double       base_spread_bp;      // baseline spread in basis points
    double       inventory_limit;     // max absolute position
};
```

```cpp
struct VenueConfig {
    VenueId      id;
    std::string  name;
    double       maker_fee_bp;
    double       taker_fee_bp;
    double       latency_ms;          // approximate venue latency
    double       cancel_penalty_bp;   // how “expensive” cancels are
};
```


### 3.2. Market Snapshot

Single‑venue snapshot:

```cpp
struct BookLevel {
    double price;
    double quantity;
};

struct VenueBookSnapshot {
    VenueId venue;
    std::vector<BookLevel> bids;
    std::vector<BookLevel> asks;
    double best_bid() const;
    double best_ask() const;
};
```

Aggregated multi‑venue view for one instrument:

```cpp
struct InstrumentMarketView {
    InstrumentId id;
    double       mid_price;        // derived fair price
    double       spread;           // best_ask - best_bid
    double       volatility;       // rolling sigma estimate
    double       weighted_depth;   // aggregate depth near mid
    std::vector<VenueBookSnapshot> venues;
};
```


### 3.3. Inventory \& P\&L

```cpp
struct InstrumentPosition {
    InstrumentId id;
    double       quantity;        // signed
    double       avg_price;       // volume-weighted
    double       realized_pnl;
    double       unrealized_pnl;
};

struct PortfolioState {
    std::unordered_map<InstrumentId, InstrumentPosition> positions;
    double total_realized_pnl;
    double total_unrealized_pnl;

    double net_exposure() const;       // sum(position * mid)
    double gross_notional() const;     // sum(|position| * mid)
};
```


***

## 4. Market Making Strategy Design

We will implement a **baseline multi‑asset market maker** with:

- Quoting at mid ± spread/2.
- Dynamic **spread widening** in high volatility / high inventory risk.
- **Inventory skew**: move quotes up/down to encourage inventory rebalancing.[^5][^3][^4]


### 4.1. Core Strategy Parameters

For each instrument:

```cpp
struct MarketMakingParams {
    double base_spread_bp;       // baseline target spread in bp of mid
    double min_spread_bp;        // floor
    double max_spread_bp;        // cap
    double volatility_coeff;     // how much to widen spread with vol
    double inventory_coeff;      // how much to skew based on inventory
    double size_base;            // base quote size
    double size_inventory_scale; // scale size vs inventory size
    double quote_refresh_ms;     // min time between re-quotes
    double max_position;         // absolute position limit
};
```


### 4.2. Quote Computation Formula

Given:

- Mid price $m$.
- Volatility estimate $\sigma$ (annualized or scaled).
- Current position $q$ and position limit $Q_{\max}$.

1. **Dynamic Spread**

$$
\text{spread} = \max(\text{min\_spread}, \min(\text{max\_spread}, s_0 + \alpha_\sigma \cdot \sigma))
$$

Where $s_0$ is base spread, $\alpha_\sigma$ is volatility coefficient.[^4][^5]

2. **Inventory Skew**

Define normalized inventory:

$$
\tilde{q} = \frac{q}{Q_{\max}}
$$

Then:

- Skew $\Delta$ in price, e.g.:

$$
\Delta = \alpha_q \cdot \tilde{q} \cdot \text{spread}
$$

Where $\alpha_q$ is inventory coefficient.[^3]

3. **Quotes**

$$
\text{bid} = m - \frac{\text{spread}}{2} - \Delta
$$

$$
\text{ask} = m + \frac{\text{spread}}{2} - \Delta
$$

- If long (positive $q$), $\tilde{q}>0$, $\Delta>0$: bid moves down, ask moves down (you sell more aggressively, buy less aggressively).
- If short, skew in opposite direction.

4. **Quote Size**

Base on inventory:

$$
\text{size} = s_0 \cdot \left(1 - \beta \cdot |\tilde{q}|\right)
$$

Clamp at min size. $\beta$ controls how quickly you shrink size as you hit limits.[^5]

***

## 5. Components and Interfaces

### 5.1. MarketDataAggregator

Responsible for building `InstrumentMarketView` from raw book updates.

```cpp
class MarketDataAggregator {
public:
    void on_book_update(const VenueBookSnapshot& snapshot);
    InstrumentMarketView get_view(InstrumentId id) const;

private:
    std::unordered_map<InstrumentId, InstrumentMarketView> views_;
    // internal state for rolling volatility, depth, etc.
};
```

Implementation tasks:

- Maintain rolling window of mid prices to compute volatility (e.g. EWMA).
- Compute `mid_price`, `spread`, `weighted_depth`.
- Support multi‑venue: per instrument, list of venue snapshots.


### 5.2. RiskManager

```cpp
class RiskManager {
public:
    explicit RiskManager(std::unordered_map<InstrumentId, MarketMakingParams> params);

    void on_fill(InstrumentId id, double price, double qty);
    bool can_quote(InstrumentId id, double bid_size, double ask_size) const;
    bool within_limits(InstrumentId id, double delta_qty) const;

    const PortfolioState& portfolio() const;

private:
    PortfolioState portfolio_;
    std::unordered_map<InstrumentId, MarketMakingParams> params_;
};
```

Responsibilities:

- Update positions/P\&L on fills.
- Enforce per‑instrument `max_position`.
- Provide methods to query current exposure for the strategy.


### 5.3. QuoteEngine

```cpp
struct Quote {
    InstrumentId id;
    VenueId      venue;
    double       bid_price;
    double       ask_price;
    double       bid_size;
    double       ask_size;
    Timestamp    ts;
};

class QuoteEngine {
public:
    QuoteEngine(const std::unordered_map<InstrumentId, MarketMakingParams>& params);

    Quote compute_quote(const InstrumentMarketView& view,
                        const InstrumentPosition& position,
                        VenueId venue) const;

private:
    std::unordered_map<InstrumentId, MarketMakingParams> params_;

    double compute_spread(const MarketMakingParams& p, double volatility) const;
    double compute_skew(const MarketMakingParams& p,
                        double inventory,
                        double max_position,
                        double spread) const;
    double compute_size(const MarketMakingParams& p,
                        double inventory,
                        double max_position) const;
};
```


### 5.4. VenueRouter

```cpp
class VenueRouter {
public:
    VenueRouter(std::vector<VenueConfig> venues);

    VenueId choose_venue(const InstrumentMarketView& view,
                         const InstrumentPosition& pos) const;

private:
    std::vector<VenueConfig> venues_;
};
```

Initial simple strategy: pick venue with lowest effective cost given maker fees and approximate latency, maybe prefer deeper venue.[^9][^5]

### 5.5. ExecutionGateway Interface

Abstract away how orders are sent:

```cpp
enum class OrderSide { Buy, Sell };

struct LiveOrder {
    uint64_t    id;
    InstrumentId instrument;
    VenueId     venue;
    OrderSide   side;
    double      price;
    double      size;
};

class IExecutionGateway {
public:
    virtual ~IExecutionGateway() = default;
    virtual uint64_t send_limit_order(const LiveOrder& order) = 0;
    virtual void     cancel_order(uint64_t order_id) = 0;
};
```

Two concrete implementations:

- `SimExecutionGateway` – talks to internal matching engine.
- `NullExecutionGateway` – for dry‑run tests.

***

## 6. Strategy Controller (Event Loop)

`MarketMakerController` ties everything together.

Responsibilities:

- On each market data update:
    - Update `MarketDataAggregator`.
    - For each instrument:
        - Get `InstrumentMarketView`.
        - Get current `InstrumentPosition` from `RiskManager`.
        - Decide whether to re‑quote (based on `quote_refresh_ms` and changes).
        - Ask `VenueRouter` for venue.
        - Use `QuoteEngine` to compute quotes.
        - Check `RiskManager::can_quote`.
        - Cancel stale quotes, send new limit orders via `IExecutionGateway`.

Skeleton:

```cpp
class MarketMakerController {
public:
    MarketMakerController(MarketDataAggregator& md,
                          RiskManager& risk,
                          QuoteEngine& qe,
                          VenueRouter& router,
                          IExecutionGateway& gw,
                          std::vector<InstrumentId> instruments);

    void on_market_data(const VenueBookSnapshot& snapshot);
    void on_fill(InstrumentId id, VenueId venue, double price, double qty);

private:
    struct InstrumentState {
        InstrumentId id;
        uint64_t     last_bid_order_id = 0;
        uint64_t     last_ask_order_id = 0;
        Timestamp    last_quote_ts     = 0;
    };

    MarketDataAggregator& md_;
    RiskManager&          risk_;
    QuoteEngine&          qe_;
    VenueRouter&          router_;
    IExecutionGateway&    gw_;
    std::unordered_map<InstrumentId, InstrumentState> state_;
};
```


***

## 7. Backtesting \& Simulation

### 7.1. Backtest Runner

Create a `BacktestRunner` that:

- Loads historical level 1 or level 2 data for multiple instruments.
- Streams them into `MarketMakerController` as `VenueBookSnapshot` events.
- Simulates fills in a simple way:
    - If the strategy posts bid/ask inside or at best levels, assume some probability of fill based on volume.
- Collects:
    - Time series of P\&L.
    - Inventory trajectory.
    - Spread captured.
    - Quote hit ratio.[^7][^6][^4]


### 7.2. Metrics \& Reporting

Create a `REPORT.md` with:

- Per‑instrument:
    - Realized P\&L, Sharpe (approx), max drawdown.
    - Inventory distribution (histogram).
    - Average effective spread captured (difference between mid at quote time and fill price).
- Global:
    - Total P\&L.
    - Max portfolio exposure.
    - Number of quotes, cancels, and fills.

***

## 8. Implementation Phases

### Phase 1 – Foundations (1 week)

- [ ] Define core data structures: `InstrumentConfig`, `VenueConfig`, `InstrumentPosition`, `PortfolioState`.
- [ ] Implement `MarketDataAggregator` with:
    - Mid price, spread, simple EWMA volatility.
- [ ] Implement `RiskManager` with positions \& limits.

**Deliverables:**

- Unit tests for volatility calculation, position updates.


### Phase 2 – Quote Engine (1 week)

- [ ] Implement `QuoteEngine` with formulas in section 4.
- [ ] Test: Give synthetic views \& positions, verify bid/ask/skew and size.
- [ ] Implement `VenueRouter` with simple cost‑based policy.

**Deliverables:**

- Deterministic tests for quote prices and sizes.


### Phase 3 – Execution Integration (1 week)

- [ ] Define `IExecutionGateway` interface.
- [ ] Implement `SimExecutionGateway` that talks to your internal matching engine.
- [ ] Implement `MarketMakerController` (event loop).

**Deliverables:**

- End‑to‑end simulation: random LOB updates → quotes → simulated fills.


### Phase 4 – Multi‑Asset \& Multi‑Venue (1 week)

- [ ] Extend to handle many instruments and venues.
- [ ] Ensure per‑instrument state and limits.
- [ ] Add config file (JSON/YAML) listing instruments and params.

**Deliverables:**

- Simulation where at least 5 instruments and 2 venues are quoted concurrently.


### Phase 5 – Backtesting \& Metrics (1–2 weeks)

- [ ] Implement `BacktestRunner`.
- [ ] Add detailed metrics and output CSV.
- [ ] Convert results into graphs (done outside C++ if needed).

**Deliverables:**

- Backtest run on at least one real or synthetic multi‑asset dataset.
- `REPORT.md` with metrics.

***

## 9. Project Structure

Suggested layout:

```text
market_making_engine/
├── CMakeLists.txt
├── README.md
├── docs/
│   ├── architecture.md
│   ├── strategy_design.md
│   └── report_example.md
├── include/
│   ├── config/
│   │   ├── instrument_config.hpp
│   │   └── venue_config.hpp
│   ├── market/
│   │   ├── market_view.hpp          // InstrumentMarketView, VenueBookSnapshot
│   │   └── market_data_aggregator.hpp
│   ├── risk/
│   │   ├── portfolio.hpp
│   │   └── risk_manager.hpp
│   ├── strategy/
│   │   ├── market_making_params.hpp
│   │   ├── quote_engine.hpp
│   │   └── market_maker_controller.hpp
│   ├── execution/
│   │   ├── execution_gateway.hpp    // interface
│   │   ├── sim_execution_gateway.hpp
│   │   └── venue_router.hpp
│   └── backtest/
│       ├── backtest_runner.hpp
│       └── metrics.hpp
├── src/
│   └── [corresponding .cpp files]
├── tests/
│   ├── unit/
│   └── integration/
└── data/
    ├── sample_lob_data.csv
    └── config.json
```


***

## 10. Success Criteria

The project is “done” when:

- ✅ The engine can quote **multiple instruments on multiple venues** concurrently.
- ✅ Inventory remains within configured limits across all instruments.
- ✅ Parameters like base spread, volatility sensitivity, and inventory skew **visibly affect** quoting behavior.
- ✅ Backtests produce meaningful P\&L and inventory curves over a historical or synthetic dataset.
- ✅ Code is modular enough that:
    - `IExecutionGateway` can be swapped to talk to real testnet/broker APIs later.
    - Strategy logic is separated from infra and matching engine.

***

**Instruction to Claude (example):**

> Use `MARKET_MAKING_ENGINE_SPEC.md` as the source of truth.
> Implement the project in phases (starting from Phase 1), following the architecture, data structures, and interfaces exactly unless changes are clearly necessary.
> Prioritize clear, testable components and keep the code ready for later integration with an external execution API.

***

This spec reflects how institutional market makers structure their C++ engines (separating quote logic, risk, and gateway) and matches design patterns described in real‑world market making discussions and tools.[^2][^1][^6][^3][^5]
<span style="display:none">[^10][^11][^12][^13][^14][^15]</span>

<div align="center">⁂</div>

[^1]: https://www.linkedin.com/pulse/evolving-c-market-making-engine-colman-marcus-quinn-s9nke

[^2]: https://www.packtpub.com/en-PT/product/building-low-latency-applications-with-c-9781837639359/chapter/chapter-10-building-the-c-plus-plus-market-making-and-liquidity-taking-algorithms-13/section/chapter-10-building-the-c-market-making-and-liquidity-taking-algorithms-ch13lvl1sec63

[^3]: https://www.linkedin.com/posts/pranay-gaurav-290a30150_how-do-institutions-build-market-making-activity-7317487083621609473-5JPC

[^4]: https://arxiv.org/pdf/1806.05101.pdf

[^5]: https://www.yellowcapital.com/blog/optimizing-order-book-depth-with-crypto-market-making/

[^6]: https://github.com/Quod-Financial/quantreplay

[^7]: https://quantreplay.com/docs/blog/building-quantreplay-realistic-multi-asset-market-simulation-for-trading-algorithm/

[^8]: https://quantreplay.com/docs/blog/inside-quantreplay-how-to-build-realistic-multi-asset-market-simulations/

[^9]: https://quantreplay.com

[^10]: https://www.reddit.com/r/cpp/comments/1d8ukso/how_do_they_use_c_in_hfthigh_frequency_trade/

[^11]: https://www.linkedin.com/posts/johannes-meyer-young-and-calculated_quantitativefinance-algorithmictrading-activity-7351172772649218051-eFFj

[^12]: https://github.com/xavierchuan/OrderMatchingEngine

[^13]: https://www.youtube.com/watch?v=sX2nF1fW7kI

[^14]: https://www.linkedin.com/pulse/why-market-makers-still-build-execution-engines-c-heath-thapa-ka6tc

[^15]: https://wp.lancs.ac.uk/finec2018/files/2018/09/FINEC-2018-028-Xiaofei.Lu_.pdf

