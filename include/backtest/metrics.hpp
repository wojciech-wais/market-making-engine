#pragma once

#include "config/instrument_config.hpp"
#include "strategy/quote_engine.hpp"

#include <vector>
#include <unordered_map>
#include <string>
#include <cmath>

namespace mme {

struct TickMetric {
    Timestamp    ts              = 0;
    InstrumentId instrument     = 0;
    double       mid_price      = 0.0;
    double       position       = 0.0;
    double       realized_pnl   = 0.0;
    double       unrealized_pnl = 0.0;
    double       bid_price      = 0.0;
    double       ask_price      = 0.0;
    double       spread_captured = 0.0;
};

struct InstrumentMetrics {
    InstrumentId id             = 0;
    double       realized_pnl   = 0.0;
    double       max_drawdown   = 0.0;
    double       sharpe_approx  = 0.0;
    double       avg_spread_captured = 0.0;
    uint64_t     total_quotes   = 0;
    uint64_t     total_fills    = 0;
    uint64_t     total_cancels  = 0;
    double       max_position   = 0.0;
    double       min_position   = 0.0;

    // P&L time series for Sharpe calculation
    std::vector<double> pnl_series;
    // Inventory trajectory
    std::vector<double> inventory_series;
};

struct GlobalMetrics {
    double   total_pnl          = 0.0;
    double   max_exposure       = 0.0;
    uint64_t total_quotes       = 0;
    uint64_t total_cancels      = 0;
    uint64_t total_fills        = 0;
};

class MetricsCollector {
public:
    void record_tick(const TickMetric& metric);
    void record_fill(InstrumentId id, double spread_captured);
    void record_quote(InstrumentId id);
    void record_cancel(InstrumentId id);
    void record_exposure(double exposure);

    InstrumentMetrics compute_instrument_metrics(InstrumentId id) const;
    GlobalMetrics compute_global_metrics() const;

    void write_csv(const std::string& filename) const;
    std::string generate_report() const;

private:
    std::unordered_map<InstrumentId, std::vector<TickMetric>> ticks_;
    std::unordered_map<InstrumentId, uint64_t> quote_counts_;
    std::unordered_map<InstrumentId, uint64_t> fill_counts_;
    std::unordered_map<InstrumentId, uint64_t> cancel_counts_;
    std::unordered_map<InstrumentId, std::vector<double>> spread_captures_;
    double max_exposure_ = 0.0;
};

} // namespace mme
