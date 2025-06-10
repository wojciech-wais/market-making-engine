#pragma once

#include "risk/portfolio.hpp"
#include "strategy/market_making_params.hpp"

#include <unordered_map>

namespace mme {

class RiskManager {
public:
    explicit RiskManager(std::unordered_map<InstrumentId, MarketMakingParams> params);

    // Update position on fill. qty is signed: positive = buy, negative = sell.
    void on_fill(InstrumentId id, double price, double qty);

    // Check if quoting is allowed given current position and proposed sizes.
    bool can_quote(InstrumentId id, double bid_size, double ask_size) const;

    // Check if adding delta_qty would stay within limits.
    bool within_limits(InstrumentId id, double delta_qty) const;

    // Update unrealized P&L given current mid prices.
    void update_unrealized(const std::unordered_map<InstrumentId, double>& mid_prices);

    const PortfolioState& portfolio() const { return portfolio_; }
    const InstrumentPosition& position(InstrumentId id) const;

private:
    PortfolioState portfolio_;
    std::unordered_map<InstrumentId, MarketMakingParams> params_;
    static const InstrumentPosition kEmptyPosition;
};

} // namespace mme
