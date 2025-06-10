#pragma once

#include "config/instrument_config.hpp"
#include "config/venue_config.hpp"

#include <vector>
#include <limits>

namespace mme {

struct BookLevel {
    double price    = 0.0;
    double quantity = 0.0;
};

struct VenueBookSnapshot {
    InstrumentId instrument = 0;
    VenueId      venue      = 0;
    std::vector<BookLevel> bids;
    std::vector<BookLevel> asks;

    double best_bid() const {
        return bids.empty() ? 0.0 : bids.front().price;
    }

    double best_ask() const {
        return asks.empty() ? std::numeric_limits<double>::max() : asks.front().price;
    }
};

struct InstrumentMarketView {
    InstrumentId id             = 0;
    double       mid_price      = 0.0;   // derived fair price
    double       spread         = 0.0;   // best_ask - best_bid
    double       volatility     = 0.0;   // rolling sigma estimate
    double       weighted_depth = 0.0;   // aggregate depth near mid
    std::vector<VenueBookSnapshot> venues;
};

} // namespace mme
