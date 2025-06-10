#include <gtest/gtest.h>
#include "execution/venue_router.hpp"

using namespace mme;

TEST(VenueRouterTest, SingleVenue) {
    std::vector<VenueConfig> venues = {
        {.id = 1, .name = "NYSE", .maker_fee_bp = 1.0, .taker_fee_bp = 2.0,
         .latency_ms = 1.0, .cancel_penalty_bp = 0.1}
    };
    VenueRouter router(venues);

    InstrumentMarketView view;
    view.id = 1;
    InstrumentPosition pos;

    EXPECT_EQ(router.choose_venue(view, pos), 1);
}

TEST(VenueRouterTest, PrefersLowerFees) {
    std::vector<VenueConfig> venues = {
        {.id = 1, .name = "Expensive", .maker_fee_bp = 5.0, .taker_fee_bp = 10.0,
         .latency_ms = 1.0, .cancel_penalty_bp = 0.5},
        {.id = 2, .name = "Cheap", .maker_fee_bp = 0.5, .taker_fee_bp = 1.0,
         .latency_ms = 1.0, .cancel_penalty_bp = 0.1},
    };
    VenueRouter router(venues);

    InstrumentMarketView view;
    view.id = 1;
    InstrumentPosition pos;

    EXPECT_EQ(router.choose_venue(view, pos), 2);
}

TEST(VenueRouterTest, DepthAffectsChoice) {
    std::vector<VenueConfig> venues = {
        {.id = 1, .name = "V1", .maker_fee_bp = 1.0, .taker_fee_bp = 2.0,
         .latency_ms = 1.0, .cancel_penalty_bp = 0.1},
        {.id = 2, .name = "V2", .maker_fee_bp = 1.0, .taker_fee_bp = 2.0,
         .latency_ms = 1.0, .cancel_penalty_bp = 0.1},
    };
    VenueRouter router(venues);

    InstrumentMarketView view;
    view.id = 1;

    // Venue 2 has much more depth
    VenueBookSnapshot snap1;
    snap1.instrument = 1;
    snap1.venue = 1;
    snap1.bids = {{99.0, 10.0}};
    snap1.asks = {{101.0, 10.0}};

    VenueBookSnapshot snap2;
    snap2.instrument = 1;
    snap2.venue = 2;
    snap2.bids = {{99.0, 1000.0}};
    snap2.asks = {{101.0, 1000.0}};

    view.venues = {snap1, snap2};
    InstrumentPosition pos;

    EXPECT_EQ(router.choose_venue(view, pos), 2);
}

TEST(VenueRouterTest, EmptyVenues) {
    VenueRouter router({});
    InstrumentMarketView view;
    InstrumentPosition pos;
    EXPECT_EQ(router.choose_venue(view, pos), 0);
}
