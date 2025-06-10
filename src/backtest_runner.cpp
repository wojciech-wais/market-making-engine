#include "backtest/backtest_runner.hpp"

#include <fstream>
#include <sstream>
#include <random>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace mme {

BacktestRunner::BacktestRunner(const BacktestConfig& config)
    : config_(config) {}

void BacktestRunner::run() {
    if (config_.data_file.empty()) {
        std::cerr << "No data file specified, use run_synthetic() instead.\n";
        return;
    }

    auto snapshots = load_csv_data(config_.data_file);
    if (snapshots.empty()) {
        std::cerr << "No data loaded from " << config_.data_file << "\n";
        return;
    }

    process_snapshots(snapshots);
}

void BacktestRunner::run_synthetic(size_t num_ticks, size_t num_instruments, size_t num_venues) {
    auto snapshots = generate_synthetic_data(num_ticks, num_instruments, num_venues);
    process_snapshots(snapshots);
}

void BacktestRunner::process_snapshots(const std::vector<VenueBookSnapshot>& snapshots) {
    // Set up components
    MarketDataAggregator md;
    RiskManager risk(config_.params);
    QuoteEngine qe(config_.params);

    std::vector<VenueConfig> venues = config_.venues;
    if (venues.empty()) {
        venues.push_back(VenueConfig{.id = 1, .name = "SIM", .maker_fee_bp = 1.0,
                                      .taker_fee_bp = 2.0, .latency_ms = 1.0, .cancel_penalty_bp = 0.1});
    }
    VenueRouter router(venues);

    // Collect instrument IDs
    std::vector<InstrumentId> instrument_ids;
    for (const auto& [id, _] : config_.params) {
        instrument_ids.push_back(id);
    }

    // Fill callback
    auto fill_cb = [&](InstrumentId id, VenueId venue, double price, double qty) {
        risk.on_fill(id, price, qty);
        auto view = md.get_view(id);
        double spread_captured = 0.0;
        if (view.mid_price > 0) {
            spread_captured = (qty > 0)
                ? (view.mid_price - price)   // bought below mid
                : (price - view.mid_price);  // sold above mid
        }
        metrics_.record_fill(id, spread_captured);
    };

    SimExecutionGateway gw(fill_cb);
    MarketMakerController controller(md, risk, qe, router, gw, instrument_ids);

    Timestamp ts = 0;

    for (const auto& snapshot : snapshots) {
        ts++;
        controller.set_current_time(ts);
        controller.on_market_data(snapshot);

        // Check for simulated fills
        gw.check_fills(snapshot);

        // Record metrics for this instrument
        auto view = md.get_view(snapshot.instrument);
        const auto& pos = risk.position(snapshot.instrument);

        metrics_.record_quote(snapshot.instrument);

        TickMetric tick{
            .ts              = ts,
            .instrument      = snapshot.instrument,
            .mid_price       = view.mid_price,
            .position        = pos.quantity,
            .realized_pnl    = pos.realized_pnl,
            .unrealized_pnl  = pos.unrealized_pnl,
            .bid_price       = view.mid_price - view.spread / 2.0,
            .ask_price       = view.mid_price + view.spread / 2.0,
            .spread_captured = 0.0,
        };
        metrics_.record_tick(tick);

        // Update unrealized P&L
        std::unordered_map<InstrumentId, double> mids;
        for (auto id : instrument_ids) {
            if (md.has_view(id)) {
                mids[id] = md.get_view(id).mid_price;
            }
        }
        risk.update_unrealized(mids);

        double exposure = risk.portfolio().net_exposure(mids);
        metrics_.record_exposure(exposure);
    }
}

std::vector<VenueBookSnapshot> BacktestRunner::load_csv_data(const std::string& filename) const {
    std::vector<VenueBookSnapshot> result;
    std::ifstream f(filename);
    if (!f.is_open()) return result;

    std::string line;
    std::getline(f, line); // skip header

    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string token;

        // Format: timestamp,instrument,venue,bid_price,bid_qty,ask_price,ask_qty
        std::vector<std::string> tokens;
        while (std::getline(iss, token, ',')) {
            tokens.push_back(token);
        }

        if (tokens.size() < 7) continue;

        VenueBookSnapshot snap;
        snap.instrument = static_cast<InstrumentId>(std::stoul(tokens[1]));
        snap.venue = static_cast<VenueId>(std::stoul(tokens[2]));
        snap.bids.push_back(BookLevel{std::stod(tokens[3]), std::stod(tokens[4])});
        snap.asks.push_back(BookLevel{std::stod(tokens[5]), std::stod(tokens[6])});

        result.push_back(std::move(snap));
    }

    return result;
}

std::vector<VenueBookSnapshot> BacktestRunner::generate_synthetic_data(
    size_t num_ticks, size_t num_instruments, size_t num_venues) const {

    std::vector<VenueBookSnapshot> result;
    result.reserve(num_ticks * num_instruments * num_venues);

    std::mt19937 rng(42); // fixed seed for reproducibility
    std::normal_distribution<double> price_move(0.0, 0.001); // ~10bp moves
    std::uniform_real_distribution<double> spread_jitter(0.8, 1.2);

    // Initialize prices for each instrument
    std::vector<double> prices(num_instruments);
    for (size_t i = 0; i < num_instruments; ++i) {
        prices[i] = 100.0 + i * 50.0; // 100, 150, 200, ...
    }

    for (size_t tick = 0; tick < num_ticks; ++tick) {
        for (size_t inst = 0; inst < num_instruments; ++inst) {
            // Random walk
            double move = price_move(rng);
            prices[inst] *= (1.0 + move);
            prices[inst] = std::max(prices[inst], 1.0); // floor

            double base_spread = prices[inst] * 0.001; // 10bp spread

            for (size_t v = 0; v < num_venues; ++v) {
                double jitter = spread_jitter(rng);
                double half_spread = base_spread * jitter / 2.0;

                VenueBookSnapshot snap;
                snap.instrument = static_cast<InstrumentId>(inst + 1);
                snap.venue = static_cast<VenueId>(v + 1);

                // 3 levels of depth
                for (int lvl = 0; lvl < 3; ++lvl) {
                    double offset = half_spread * (1.0 + lvl * 0.5);
                    double qty = 10.0 + lvl * 5.0;
                    snap.bids.push_back(BookLevel{prices[inst] - offset, qty});
                    snap.asks.push_back(BookLevel{prices[inst] + offset, qty});
                }

                result.push_back(std::move(snap));
            }
        }
    }

    return result;
}

void BacktestRunner::write_report(const std::string& report_path) const {
    std::ofstream f(report_path);
    f << metrics_.generate_report();
}

void BacktestRunner::write_csv(const std::string& csv_path) const {
    metrics_.write_csv(csv_path);
}

} // namespace mme
