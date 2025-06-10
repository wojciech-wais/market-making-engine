#include <gtest/gtest.h>
#include "market/market_data_aggregator.hpp"
#include "risk/risk_manager.hpp"
#include "strategy/quote_engine.hpp"
#include "strategy/market_maker_controller.hpp"
#include "execution/sim_execution_gateway.hpp"
#include "execution/venue_router.hpp"
#include "backtest/backtest_runner.hpp"

using namespace mme;

class EndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        MarketMakingParams params;
        params.base_spread_bp = 10.0;
        params.min_spread_bp = 2.0;
        params.max_spread_bp = 50.0;
        params.volatility_coeff = 1.0;
        params.inventory_coeff = 0.5;
        params.size_base = 5.0;
        params.size_inventory_scale = 0.5;
        params.max_position = 100.0;

        for (InstrumentId id = 1; id <= 3; ++id) {
            params_map[id] = params;
            instruments.push_back(id);
        }

        venues.push_back(VenueConfig{.id = 1, .name = "V1", .maker_fee_bp = 1.0,
                                      .taker_fee_bp = 2.0, .latency_ms = 0.5, .cancel_penalty_bp = 0.1});
        venues.push_back(VenueConfig{.id = 2, .name = "V2", .maker_fee_bp = 1.5,
                                      .taker_fee_bp = 2.5, .latency_ms = 1.0, .cancel_penalty_bp = 0.2});
    }

    std::unordered_map<InstrumentId, MarketMakingParams> params_map;
    std::vector<InstrumentId> instruments;
    std::vector<VenueConfig> venues;
};

TEST_F(EndToEndTest, ControllerQuotesOnMarketData) {
    MarketDataAggregator md;
    RiskManager risk(params_map);
    QuoteEngine qe(params_map);
    VenueRouter router(venues);

    int fill_count = 0;
    SimExecutionGateway gw([&](InstrumentId id, VenueId, double price, double qty) {
        risk.on_fill(id, price, qty);
        fill_count++;
    });

    MarketMakerController controller(md, risk, qe, router, gw, instruments);

    // Send market data updates
    VenueBookSnapshot snap;
    snap.instrument = 1;
    snap.venue = 1;
    snap.bids = {{99.5, 10.0}, {99.0, 20.0}};
    snap.asks = {{100.5, 10.0}, {101.0, 20.0}};

    controller.on_market_data(snap);

    // Gateway should have active orders
    EXPECT_GT(gw.active_order_count(), 0u);
}

TEST_F(EndToEndTest, MultiInstrumentQuoting) {
    MarketDataAggregator md;
    RiskManager risk(params_map);
    QuoteEngine qe(params_map);
    VenueRouter router(venues);

    SimExecutionGateway gw([&](InstrumentId id, VenueId, double price, double qty) {
        risk.on_fill(id, price, qty);
    });

    MarketMakerController controller(md, risk, qe, router, gw, instruments);

    // Send updates for multiple instruments
    for (InstrumentId id = 1; id <= 3; ++id) {
        VenueBookSnapshot snap;
        snap.instrument = id;
        snap.venue = 1;
        double base = 100.0 + id * 50.0;
        snap.bids = {{base - 0.5, 10.0}};
        snap.asks = {{base + 0.5, 10.0}};
        controller.on_market_data(snap);
    }

    // Should have orders for all 3 instruments (2 per instrument = 6)
    EXPECT_GE(gw.active_order_count(), 3u);
}

TEST_F(EndToEndTest, FillUpdatesInventory) {
    MarketDataAggregator md;
    RiskManager risk(params_map);
    QuoteEngine qe(params_map);
    VenueRouter router(venues);

    SimExecutionGateway gw([&](InstrumentId id, VenueId, double price, double qty) {
        risk.on_fill(id, price, qty);
    });

    MarketMakerController controller(md, risk, qe, router, gw, instruments);

    // Manually trigger a fill
    risk.on_fill(1, 100.0, 5.0);
    EXPECT_DOUBLE_EQ(risk.position(1).quantity, 5.0);

    risk.on_fill(1, 100.0, -5.0);
    EXPECT_DOUBLE_EQ(risk.position(1).quantity, 0.0);
}

TEST_F(EndToEndTest, InventoryLimitsPreventQuoting) {
    MarketMakingParams tight_params;
    tight_params.max_position = 5.0;
    tight_params.size_base = 10.0; // larger than limit
    std::unordered_map<InstrumentId, MarketMakingParams> pm;
    pm[1] = tight_params;

    RiskManager risk(pm);

    // Fill to near limit
    risk.on_fill(1, 100.0, 4.0);

    // Should not be within limits for a large buy
    EXPECT_FALSE(risk.within_limits(1, 10.0));
    // But small additions ok
    EXPECT_TRUE(risk.within_limits(1, 1.0));
}

TEST_F(EndToEndTest, BacktestRunnerSynthetic) {
    BacktestConfig config;
    config.venues = venues;
    for (auto& [id, p] : params_map) {
        config.params[id] = p;
        config.instruments.push_back(InstrumentConfig{
            .id = id, .symbol = "SYM" + std::to_string(id),
            .tick_size = 0.01, .lot_size = 1.0,
            .base_spread_bp = 10.0, .inventory_limit = 100.0});
    }

    BacktestRunner runner(config);
    runner.run_synthetic(100, 3, 2); // 100 ticks, 3 instruments, 2 venues

    auto global = runner.metrics().compute_global_metrics();
    EXPECT_GT(global.total_quotes, 0u);

    // Metrics should be computable
    for (InstrumentId id = 1; id <= 3; ++id) {
        auto m = runner.metrics().compute_instrument_metrics(id);
        EXPECT_EQ(m.id, id);
    }
}

TEST_F(EndToEndTest, BacktestGeneratesReport) {
    BacktestConfig config;
    config.venues = venues;
    for (auto& [id, p] : params_map) {
        config.params[id] = p;
        config.instruments.push_back(InstrumentConfig{
            .id = id, .symbol = "SYM" + std::to_string(id),
            .tick_size = 0.01, .lot_size = 1.0,
            .base_spread_bp = 10.0, .inventory_limit = 100.0});
    }

    BacktestRunner runner(config);
    runner.run_synthetic(500, 3, 2);

    auto report = runner.metrics().generate_report();
    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("Market Making Backtest Report"), std::string::npos);
    EXPECT_NE(report.find("Global Metrics"), std::string::npos);
    EXPECT_NE(report.find("Per-Instrument Metrics"), std::string::npos);
}
