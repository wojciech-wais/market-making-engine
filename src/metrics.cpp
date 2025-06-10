#include "backtest/metrics.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iomanip>

namespace mme {

void MetricsCollector::record_tick(const TickMetric& metric) {
    ticks_[metric.instrument].push_back(metric);
}

void MetricsCollector::record_fill(InstrumentId id, double spread_captured) {
    fill_counts_[id]++;
    spread_captures_[id].push_back(spread_captured);
}

void MetricsCollector::record_quote(InstrumentId id) {
    quote_counts_[id]++;
}

void MetricsCollector::record_cancel(InstrumentId id) {
    cancel_counts_[id]++;
}

void MetricsCollector::record_exposure(double exposure) {
    max_exposure_ = std::max(max_exposure_, std::abs(exposure));
}

InstrumentMetrics MetricsCollector::compute_instrument_metrics(InstrumentId id) const {
    InstrumentMetrics m;
    m.id = id;

    auto ticks_it = ticks_.find(id);
    if (ticks_it == ticks_.end() || ticks_it->second.empty()) return m;

    const auto& ticks = ticks_it->second;

    // P&L and inventory series
    double peak_pnl = 0.0;
    double max_dd = 0.0;
    double max_pos = 0.0;
    double min_pos = 0.0;

    for (const auto& t : ticks) {
        double total_pnl = t.realized_pnl + t.unrealized_pnl;
        m.pnl_series.push_back(total_pnl);
        m.inventory_series.push_back(t.position);

        peak_pnl = std::max(peak_pnl, total_pnl);
        max_dd = std::max(max_dd, peak_pnl - total_pnl);
        max_pos = std::max(max_pos, t.position);
        min_pos = std::min(min_pos, t.position);
    }

    m.realized_pnl = ticks.back().realized_pnl;
    m.max_drawdown = max_dd;
    m.max_position = max_pos;
    m.min_position = min_pos;

    // Sharpe approximation from P&L differences
    if (m.pnl_series.size() > 1) {
        std::vector<double> returns;
        for (size_t i = 1; i < m.pnl_series.size(); ++i) {
            returns.push_back(m.pnl_series[i] - m.pnl_series[i - 1]);
        }
        double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
        double sq_sum = 0.0;
        for (double r : returns) sq_sum += (r - mean) * (r - mean);
        double stddev = std::sqrt(sq_sum / returns.size());
        m.sharpe_approx = (stddev > 1e-12) ? (mean / stddev) * std::sqrt(252.0) : 0.0;
    }

    // Average spread captured
    auto sc_it = spread_captures_.find(id);
    if (sc_it != spread_captures_.end() && !sc_it->second.empty()) {
        m.avg_spread_captured = std::accumulate(sc_it->second.begin(), sc_it->second.end(), 0.0)
                                / sc_it->second.size();
    }

    auto qc_it = quote_counts_.find(id);
    m.total_quotes = (qc_it != quote_counts_.end()) ? qc_it->second : 0;

    auto fc_it = fill_counts_.find(id);
    m.total_fills = (fc_it != fill_counts_.end()) ? fc_it->second : 0;

    auto cc_it = cancel_counts_.find(id);
    m.total_cancels = (cc_it != cancel_counts_.end()) ? cc_it->second : 0;

    return m;
}

GlobalMetrics MetricsCollector::compute_global_metrics() const {
    GlobalMetrics g;
    g.max_exposure = max_exposure_;

    for (const auto& [id, _] : ticks_) {
        auto m = compute_instrument_metrics(id);
        g.total_pnl += m.realized_pnl;
        g.total_quotes += m.total_quotes;
        g.total_fills += m.total_fills;
        g.total_cancels += m.total_cancels;
    }

    return g;
}

void MetricsCollector::write_csv(const std::string& filename) const {
    std::ofstream f(filename);
    f << "timestamp,instrument,mid_price,position,realized_pnl,unrealized_pnl,bid_price,ask_price,spread_captured\n";

    for (const auto& [id, ticks] : ticks_) {
        for (const auto& t : ticks) {
            f << t.ts << ","
              << t.instrument << ","
              << std::fixed << std::setprecision(6)
              << t.mid_price << ","
              << t.position << ","
              << t.realized_pnl << ","
              << t.unrealized_pnl << ","
              << t.bid_price << ","
              << t.ask_price << ","
              << t.spread_captured << "\n";
        }
    }
}

std::string MetricsCollector::generate_report() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);

    ss << "# Market Making Backtest Report\n\n";

    // Global metrics
    auto g = compute_global_metrics();
    ss << "## Global Metrics\n\n";
    ss << "| Metric | Value |\n";
    ss << "|--------|-------|\n";
    ss << "| Total P&L | " << g.total_pnl << " |\n";
    ss << "| Max Portfolio Exposure | " << g.max_exposure << " |\n";
    ss << "| Total Quotes | " << g.total_quotes << " |\n";
    ss << "| Total Cancels | " << g.total_cancels << " |\n";
    ss << "| Total Fills | " << g.total_fills << " |\n";
    ss << "\n";

    // Per-instrument metrics
    ss << "## Per-Instrument Metrics\n\n";
    ss << "| Instrument | Realized P&L | Sharpe | Max DD | Avg Spread Captured | Quotes | Fills | Max Pos | Min Pos |\n";
    ss << "|------------|-------------|--------|--------|---------------------|--------|-------|---------|--------|\n";

    for (const auto& [id, _] : ticks_) {
        auto m = compute_instrument_metrics(id);
        ss << "| " << m.id
           << " | " << m.realized_pnl
           << " | " << m.sharpe_approx
           << " | " << m.max_drawdown
           << " | " << m.avg_spread_captured
           << " | " << m.total_quotes
           << " | " << m.total_fills
           << " | " << m.max_position
           << " | " << m.min_position
           << " |\n";
    }

    return ss.str();
}

} // namespace mme
