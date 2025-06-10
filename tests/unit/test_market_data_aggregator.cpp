#include <gtest/gtest.h>
#include "market/market_data_aggregator.hpp"

using namespace mme;

class MarketDataAggregatorTest : public ::testing::Test {
protected:
    MarketDataAggregator agg{0.1}; // higher alpha for faster response in tests
};

TEST_F(MarketDataAggregatorTest, EmptyView) {
    auto view = agg.get_view(1);
    EXPECT_EQ(view.id, 1);
    EXPECT_DOUBLE_EQ(view.mid_price, 0.0);
    EXPECT_FALSE(agg.has_view(1));
}

TEST_F(MarketDataAggregatorTest, SingleUpdate) {
    VenueBookSnapshot snap;
    snap.instrument = 1;
    snap.venue = 1;
    snap.bids = {{99.0, 10.0}};
    snap.asks = {{101.0, 10.0}};

    agg.on_book_update(snap);

    EXPECT_TRUE(agg.has_view(1));
    auto view = agg.get_view(1);
    EXPECT_EQ(view.id, 1);
    EXPECT_DOUBLE_EQ(view.mid_price, 100.0);
    EXPECT_DOUBLE_EQ(view.spread, 2.0);
    EXPECT_EQ(view.venues.size(), 1);
}

TEST_F(MarketDataAggregatorTest, MultiVenueAggregation) {
    VenueBookSnapshot snap1;
    snap1.instrument = 1;
    snap1.venue = 1;
    snap1.bids = {{99.0, 10.0}};
    snap1.asks = {{101.0, 10.0}};

    VenueBookSnapshot snap2;
    snap2.instrument = 1;
    snap2.venue = 2;
    snap2.bids = {{99.5, 15.0}}; // better bid
    snap2.asks = {{100.5, 15.0}}; // better ask

    agg.on_book_update(snap1);
    agg.on_book_update(snap2);

    auto view = agg.get_view(1);
    EXPECT_EQ(view.venues.size(), 2);
    // Global best bid should be 99.5 (venue 2), best ask 100.5 (venue 2)
    EXPECT_DOUBLE_EQ(view.mid_price, 100.0);
    EXPECT_DOUBLE_EQ(view.spread, 1.0);
}

TEST_F(MarketDataAggregatorTest, VolatilityUpdate) {
    // Feed a series of updates with increasing prices to build volatility
    for (int i = 0; i < 20; ++i) {
        VenueBookSnapshot snap;
        snap.instrument = 1;
        snap.venue = 1;
        double base = 100.0 + i * 0.1;
        snap.bids = {{base - 0.5, 10.0}};
        snap.asks = {{base + 0.5, 10.0}};
        agg.on_book_update(snap);
    }

    auto view = agg.get_view(1);
    // Volatility should be non-zero after multiple updates
    EXPECT_GT(view.volatility, 0.0);
    EXPECT_GT(view.mid_price, 100.0);
}

TEST_F(MarketDataAggregatorTest, WeightedDepth) {
    VenueBookSnapshot snap;
    snap.instrument = 1;
    snap.venue = 1;
    snap.bids = {{99.0, 10.0}, {98.5, 20.0}, {98.0, 30.0}};
    snap.asks = {{101.0, 10.0}, {101.5, 20.0}, {102.0, 30.0}};

    agg.on_book_update(snap);

    auto view = agg.get_view(1);
    // Depth should include top 3 levels from both sides: (10+20+30)*2 = 120
    EXPECT_DOUBLE_EQ(view.weighted_depth, 120.0);
}

TEST_F(MarketDataAggregatorTest, VenueSnapshotReplacement) {
    VenueBookSnapshot snap1;
    snap1.instrument = 1;
    snap1.venue = 1;
    snap1.bids = {{99.0, 10.0}};
    snap1.asks = {{101.0, 10.0}};
    agg.on_book_update(snap1);

    // Update same venue
    VenueBookSnapshot snap2;
    snap2.instrument = 1;
    snap2.venue = 1;
    snap2.bids = {{99.5, 15.0}};
    snap2.asks = {{100.5, 15.0}};
    agg.on_book_update(snap2);

    auto view = agg.get_view(1);
    EXPECT_EQ(view.venues.size(), 1); // should replace, not add
    EXPECT_DOUBLE_EQ(view.mid_price, 100.0);
    EXPECT_DOUBLE_EQ(view.spread, 1.0);
}
