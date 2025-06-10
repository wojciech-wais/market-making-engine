#pragma once

#include "config/instrument_config.hpp"
#include "config/venue_config.hpp"
#include "market/market_view.hpp"
#include "risk/portfolio.hpp"
#include "strategy/market_making_params.hpp"

#include <chrono>
#include <unordered_map>

namespace mme {

using Timestamp = uint64_t; // milliseconds since epoch

struct Quote {
    InstrumentId id        = 0;
    VenueId      venue     = 0;
    double       bid_price = 0.0;
    double       ask_price = 0.0;
    double       bid_size  = 0.0;
    double       ask_size  = 0.0;
    Timestamp    ts        = 0;
};

class QuoteEngine {
public:
    explicit QuoteEngine(std::unordered_map<InstrumentId, MarketMakingParams> params);

    Quote compute_quote(const InstrumentMarketView& view,
                        const InstrumentPosition& position,
                        VenueId venue) const;

private:
    std::unordered_map<InstrumentId, MarketMakingParams> params_;

    double compute_spread(const MarketMakingParams& p, double volatility) const;
    double compute_skew(const MarketMakingParams& p,
                        double inventory,
                        double max_position,
                        double spread) const;
    double compute_size(const MarketMakingParams& p,
                        double inventory,
                        double max_position) const;
};

} // namespace mme
