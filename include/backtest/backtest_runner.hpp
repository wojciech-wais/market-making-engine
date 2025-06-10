#pragma once

#include "strategy/market_maker_controller.hpp"
#include "execution/sim_execution_gateway.hpp"
#include "backtest/metrics.hpp"

#include <string>
#include <vector>
#include <functional>

namespace mme {

struct BacktestConfig {
    std::vector<InstrumentConfig> instruments;
    std::vector<VenueConfig>      venues;
    std::unordered_map<InstrumentId, MarketMakingParams> params;
    std::string data_file;      // path to CSV data file
    double fill_probability = 0.3; // probability of fill when at best level
};

class BacktestRunner {
public:
    explicit BacktestRunner(const BacktestConfig& config);

    // Run backtest on loaded data
    void run();

    // Run backtest on synthetic data (generates random walk LOB updates)
    void run_synthetic(size_t num_ticks, size_t num_instruments = 5, size_t num_venues = 2);

    const MetricsCollector& metrics() const { return metrics_; }

    // Generate report and CSV
    void write_report(const std::string& report_path) const;
    void write_csv(const std::string& csv_path) const;

private:
    // Load CSV data: timestamp,instrument,venue,bid_price,bid_qty,ask_price,ask_qty
    std::vector<VenueBookSnapshot> load_csv_data(const std::string& filename) const;

    // Generate synthetic book data
    std::vector<VenueBookSnapshot> generate_synthetic_data(
        size_t num_ticks, size_t num_instruments, size_t num_venues) const;

    void process_snapshots(const std::vector<VenueBookSnapshot>& snapshots);

    BacktestConfig config_;
    MetricsCollector metrics_;
};

} // namespace mme
