#include <gtest/gtest.h>
#include "strategy/quote_engine.hpp"

using namespace mme;

class QuoteEngineTest : public ::testing::Test {
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

        std::unordered_map<InstrumentId, MarketMakingParams> pm;
        pm[1] = params;
        qe = std::make_unique<QuoteEngine>(std::move(pm));
    }

    std::unique_ptr<QuoteEngine> qe;
};

TEST_F(QuoteEngineTest, BasicQuoteSymmetric) {
    InstrumentMarketView view;
    view.id = 1;
    view.mid_price = 100.0;
    view.volatility = 0.0;

    InstrumentPosition pos;
    pos.id = 1;
    pos.quantity = 0.0;

    auto quote = qe->compute_quote(view, pos, 1);

    EXPECT_EQ(quote.id, 1);
    EXPECT_EQ(quote.venue, 1);

    // With 0 volatility and 0 inventory: spread = base_spread_bp = 10bp
    // 10bp of 100 = 0.10
    double expected_half_spread = 0.05;
    EXPECT_NEAR(quote.bid_price, 100.0 - expected_half_spread, 0.001);
    EXPECT_NEAR(quote.ask_price, 100.0 + expected_half_spread, 0.001);

    // Symmetric: bid and ask equidistant from mid
    EXPECT_NEAR(quote.ask_price - quote.bid_price, 0.10, 0.001);

    // Full size with 0 inventory
    EXPECT_DOUBLE_EQ(quote.bid_size, 5.0);
    EXPECT_DOUBLE_EQ(quote.ask_size, 5.0);
}

TEST_F(QuoteEngineTest, VolatilityWidensSpread) {
    InstrumentMarketView view;
    view.id = 1;
    view.mid_price = 100.0;
    view.volatility = 0.001; // 10bp volatility

    InstrumentPosition pos{.id = 1, .quantity = 0.0};

    auto quote = qe->compute_quote(view, pos, 1);

    // spread = base_spread(10) + vol_coeff(1.0) * vol_bp(10) = 20bp
    double expected_spread = 20.0 * 100.0 / 10000.0; // 0.20
    EXPECT_NEAR(quote.ask_price - quote.bid_price, expected_spread, 0.001);
}

TEST_F(QuoteEngineTest, SpreadClamping) {
    InstrumentMarketView view;
    view.id = 1;
    view.mid_price = 100.0;
    view.volatility = 0.1; // Very high volatility: 1000bp

    InstrumentPosition pos{.id = 1, .quantity = 0.0};

    auto quote = qe->compute_quote(view, pos, 1);

    // Should be clamped at max_spread_bp = 50bp
    double max_spread = 50.0 * 100.0 / 10000.0; // 0.50
    EXPECT_NEAR(quote.ask_price - quote.bid_price, max_spread, 0.001);
}

TEST_F(QuoteEngineTest, InventorySkewLong) {
    InstrumentMarketView view;
    view.id = 1;
    view.mid_price = 100.0;
    view.volatility = 0.0;

    InstrumentPosition pos{.id = 1, .quantity = 50.0}; // 50% of max

    auto quote = qe->compute_quote(view, pos, 1);

    // Skew = inventory_coeff(0.5) * q_tilde(0.5) * spread
    // When long, both bid and ask move down (skew > 0)
    double spread_abs = 10.0 * 100.0 / 10000.0; // 0.10
    double skew = 0.5 * 0.5 * spread_abs; // 0.025
    EXPECT_NEAR(quote.bid_price, 100.0 - spread_abs / 2.0 - skew, 0.001);
    EXPECT_NEAR(quote.ask_price, 100.0 + spread_abs / 2.0 - skew, 0.001);

    // Ask is lower (more aggressive selling), bid is lower (less aggressive buying)
    EXPECT_LT(quote.ask_price, 100.0 + spread_abs / 2.0);
}

TEST_F(QuoteEngineTest, InventorySkewShort) {
    InstrumentMarketView view;
    view.id = 1;
    view.mid_price = 100.0;
    view.volatility = 0.0;

    InstrumentPosition pos{.id = 1, .quantity = -50.0}; // 50% short

    auto quote = qe->compute_quote(view, pos, 1);

    // When short, skew < 0 → both bid and ask move up
    double spread_abs = 10.0 * 100.0 / 10000.0; // 0.10
    EXPECT_GT(quote.bid_price, 100.0 - spread_abs / 2.0);
}

TEST_F(QuoteEngineTest, SizeReducesWithInventory) {
    InstrumentMarketView view;
    view.id = 1;
    view.mid_price = 100.0;
    view.volatility = 0.0;

    // Zero inventory → full size
    InstrumentPosition pos0{.id = 1, .quantity = 0.0};
    auto q0 = qe->compute_quote(view, pos0, 1);
    EXPECT_DOUBLE_EQ(q0.bid_size, 5.0);

    // Half inventory → reduced size
    InstrumentPosition pos50{.id = 1, .quantity = 50.0};
    auto q50 = qe->compute_quote(view, pos50, 1);
    // size = 5.0 * (1 - 0.5 * 0.5) = 5.0 * 0.75 = 3.75
    EXPECT_NEAR(q50.bid_size, 3.75, 0.01);

    // Full inventory → min size (compute_size gives 2.5, then near-limit
    // reduction: normalized_inv=1.0 > 0.8 → bid_size *= max(0.1, 1-1.0) = 0.1)
    InstrumentPosition pos100{.id = 1, .quantity = 100.0};
    auto q100 = qe->compute_quote(view, pos100, 1);
    EXPECT_NEAR(q100.bid_size, 2.5 * 0.1, 0.01);
}

TEST_F(QuoteEngineTest, ZeroMidReturnsEmpty) {
    InstrumentMarketView view;
    view.id = 1;
    view.mid_price = 0.0;

    InstrumentPosition pos{.id = 1};

    auto quote = qe->compute_quote(view, pos, 1);
    EXPECT_DOUBLE_EQ(quote.bid_price, 0.0);
    EXPECT_DOUBLE_EQ(quote.ask_price, 0.0);
}

TEST_F(QuoteEngineTest, UnknownInstrument) {
    InstrumentMarketView view;
    view.id = 999; // not configured
    view.mid_price = 100.0;

    InstrumentPosition pos{.id = 999};

    auto quote = qe->compute_quote(view, pos, 1);
    EXPECT_DOUBLE_EQ(quote.bid_price, 0.0);
}
