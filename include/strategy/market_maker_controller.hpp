#pragma once

#include "market/market_data_aggregator.hpp"
#include "risk/risk_manager.hpp"
#include "strategy/quote_engine.hpp"
#include "execution/venue_router.hpp"
#include "execution/execution_gateway.hpp"

#include <vector>
#include <unordered_map>

namespace mme {

class MarketMakerController {
public:
    MarketMakerController(MarketDataAggregator& md,
                          RiskManager& risk,
                          QuoteEngine& qe,
                          VenueRouter& router,
                          IExecutionGateway& gw,
                          std::vector<InstrumentId> instruments);

    void on_market_data(const VenueBookSnapshot& snapshot);
    void on_fill(InstrumentId id, VenueId venue, double price, double qty);

    // Set current timestamp (for simulation use)
    void set_current_time(Timestamp ts) { current_time_ = ts; }

private:
    struct InstrumentState {
        InstrumentId id           = 0;
        uint64_t     last_bid_order_id = 0;
        uint64_t     last_ask_order_id = 0;
        Timestamp    last_quote_ts     = 0;
    };

    void try_requote(InstrumentId id);

    MarketDataAggregator& md_;
    RiskManager&          risk_;
    QuoteEngine&          qe_;
    VenueRouter&          router_;
    IExecutionGateway&    gw_;
    std::unordered_map<InstrumentId, InstrumentState> state_;
    Timestamp current_time_ = 0;
};

} // namespace mme
