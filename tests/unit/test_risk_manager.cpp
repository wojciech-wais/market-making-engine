#include <gtest/gtest.h>
#include "risk/risk_manager.hpp"

using namespace mme;

class RiskManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        MarketMakingParams params;
        params.max_position = 100.0;
        params.size_base = 5.0;
        std::unordered_map<InstrumentId, MarketMakingParams> pm;
        pm[1] = params;
        pm[2] = params;
        risk = std::make_unique<RiskManager>(std::move(pm));
    }

    std::unique_ptr<RiskManager> risk;
};

TEST_F(RiskManagerTest, InitialPositionEmpty) {
    const auto& pos = risk->position(1);
    EXPECT_DOUBLE_EQ(pos.quantity, 0.0);
    EXPECT_DOUBLE_EQ(pos.realized_pnl, 0.0);
}

TEST_F(RiskManagerTest, BuyFillUpdatesPosition) {
    risk->on_fill(1, 100.0, 10.0); // buy 10 at 100
    const auto& pos = risk->position(1);
    EXPECT_DOUBLE_EQ(pos.quantity, 10.0);
    EXPECT_DOUBLE_EQ(pos.avg_price, 100.0);
}

TEST_F(RiskManagerTest, MultipleBuyFills) {
    risk->on_fill(1, 100.0, 10.0); // buy 10 at 100
    risk->on_fill(1, 102.0, 10.0); // buy 10 at 102
    const auto& pos = risk->position(1);
    EXPECT_DOUBLE_EQ(pos.quantity, 20.0);
    EXPECT_DOUBLE_EQ(pos.avg_price, 101.0); // (100*10 + 102*10) / 20
}

TEST_F(RiskManagerTest, BuySellRealizesPnL) {
    risk->on_fill(1, 100.0, 10.0); // buy 10 at 100
    risk->on_fill(1, 105.0, -10.0); // sell 10 at 105
    const auto& pos = risk->position(1);
    EXPECT_DOUBLE_EQ(pos.quantity, 0.0);
    EXPECT_DOUBLE_EQ(pos.realized_pnl, 50.0); // (105-100)*10
}

TEST_F(RiskManagerTest, ShortSellRealizesPnL) {
    risk->on_fill(1, 100.0, -10.0); // sell 10 at 100
    risk->on_fill(1, 95.0, 10.0);   // buy 10 at 95
    const auto& pos = risk->position(1);
    EXPECT_DOUBLE_EQ(pos.quantity, 0.0);
    EXPECT_DOUBLE_EQ(pos.realized_pnl, 50.0); // (100-95)*10
}

TEST_F(RiskManagerTest, WithinLimits) {
    EXPECT_TRUE(risk->within_limits(1, 50.0));
    EXPECT_TRUE(risk->within_limits(1, 100.0));
    EXPECT_FALSE(risk->within_limits(1, 101.0));
}

TEST_F(RiskManagerTest, WithinLimitsAfterFill) {
    risk->on_fill(1, 100.0, 90.0); // buy 90
    EXPECT_TRUE(risk->within_limits(1, 10.0));  // 90+10=100, at limit
    EXPECT_FALSE(risk->within_limits(1, 11.0)); // 90+11=101, over limit
    EXPECT_TRUE(risk->within_limits(1, -10.0)); // 90-10=80, ok
}

TEST_F(RiskManagerTest, CanQuote) {
    EXPECT_TRUE(risk->can_quote(1, 5.0, 5.0));

    risk->on_fill(1, 100.0, 98.0); // near long limit
    // Bid would push to 103, ask would reduce to 93
    EXPECT_TRUE(risk->can_quote(1, 5.0, 5.0)); // at least one side is ok
}

TEST_F(RiskManagerTest, UnrealizedPnL) {
    risk->on_fill(1, 100.0, 10.0);
    std::unordered_map<InstrumentId, double> mids;
    mids[1] = 105.0;
    risk->update_unrealized(mids);
    const auto& pos = risk->position(1);
    EXPECT_DOUBLE_EQ(pos.unrealized_pnl, 50.0); // (105-100)*10
}

TEST_F(RiskManagerTest, TotalPortfolioPnL) {
    risk->on_fill(1, 100.0, 10.0);
    risk->on_fill(2, 200.0, 5.0);
    risk->on_fill(1, 110.0, -10.0); // close instrument 1
    EXPECT_DOUBLE_EQ(risk->portfolio().total_realized_pnl, 100.0); // (110-100)*10
}

TEST_F(RiskManagerTest, UnknownInstrumentWithinLimits) {
    EXPECT_FALSE(risk->within_limits(999, 1.0)); // unknown instrument
}
