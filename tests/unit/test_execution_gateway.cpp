#include <gtest/gtest.h>
#include "execution/sim_execution_gateway.hpp"

using namespace mme;

TEST(SimExecutionGatewayTest, SendAndCancel) {
    bool filled = false;
    SimExecutionGateway gw([&](InstrumentId, VenueId, double, double) {
        filled = true;
    });

    LiveOrder order{.id = 0, .instrument = 1, .venue = 1,
                    .side = OrderSide::Buy, .price = 99.0, .size = 10.0};

    uint64_t id = gw.send_limit_order(order);
    EXPECT_GT(id, 0u);
    EXPECT_EQ(gw.active_order_count(), 1);

    gw.cancel_order(id);
    EXPECT_EQ(gw.active_order_count(), 0);
    EXPECT_FALSE(filled);
}

TEST(SimExecutionGatewayTest, BuyFillWhenAskCrosses) {
    InstrumentId fill_inst = 0;
    double fill_price = 0.0;
    double fill_qty = 0.0;

    SimExecutionGateway gw([&](InstrumentId id, VenueId, double price, double qty) {
        fill_inst = id;
        fill_price = price;
        fill_qty = qty;
    });

    LiveOrder buy{.id = 0, .instrument = 1, .venue = 1,
                  .side = OrderSide::Buy, .price = 100.0, .size = 5.0};
    gw.send_limit_order(buy);

    // Market ask at 99.5 → crosses our bid at 100
    VenueBookSnapshot snap;
    snap.instrument = 1;
    snap.venue = 1;
    snap.bids = {{98.0, 10.0}};
    snap.asks = {{99.5, 10.0}};
    gw.check_fills(snap);

    EXPECT_EQ(fill_inst, 1u);
    EXPECT_DOUBLE_EQ(fill_price, 100.0);
    EXPECT_DOUBLE_EQ(fill_qty, 5.0); // positive = buy
    EXPECT_EQ(gw.active_order_count(), 0); // filled order removed
}

TEST(SimExecutionGatewayTest, SellFillWhenBidCrosses) {
    double fill_qty = 0.0;

    SimExecutionGateway gw([&](InstrumentId, VenueId, double, double qty) {
        fill_qty = qty;
    });

    LiveOrder sell{.id = 0, .instrument = 1, .venue = 1,
                   .side = OrderSide::Sell, .price = 100.0, .size = 5.0};
    gw.send_limit_order(sell);

    // Market bid at 100.5 → crosses our ask at 100
    VenueBookSnapshot snap;
    snap.instrument = 1;
    snap.venue = 1;
    snap.bids = {{100.5, 10.0}};
    snap.asks = {{102.0, 10.0}};
    gw.check_fills(snap);

    EXPECT_DOUBLE_EQ(fill_qty, -5.0); // negative = sell
}

TEST(SimExecutionGatewayTest, NoFillWhenNoCross) {
    bool filled = false;

    SimExecutionGateway gw([&](InstrumentId, VenueId, double, double) {
        filled = true;
    });

    LiveOrder buy{.id = 0, .instrument = 1, .venue = 1,
                  .side = OrderSide::Buy, .price = 99.0, .size = 5.0};
    gw.send_limit_order(buy);

    // Ask at 101 → no cross with bid at 99
    VenueBookSnapshot snap;
    snap.instrument = 1;
    snap.venue = 1;
    snap.bids = {{98.0, 10.0}};
    snap.asks = {{101.0, 10.0}};
    gw.check_fills(snap);

    EXPECT_FALSE(filled);
    EXPECT_EQ(gw.active_order_count(), 1);
}

TEST(NullExecutionGatewayTest, CountsOrders) {
    NullExecutionGateway gw;

    LiveOrder order{.id = 0, .instrument = 1, .venue = 1,
                    .side = OrderSide::Buy, .price = 100.0, .size = 5.0};

    uint64_t id1 = gw.send_limit_order(order);
    uint64_t id2 = gw.send_limit_order(order);
    gw.cancel_order(id1);

    EXPECT_EQ(gw.orders_sent(), 2u);
    EXPECT_EQ(gw.cancels_sent(), 1u);
    EXPECT_NE(id1, id2);
}
