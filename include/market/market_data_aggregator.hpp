#pragma once

#include "market/market_view.hpp"
#include "config/instrument_config.hpp"

#include <unordered_map>
#include <deque>

namespace mme {

class MarketDataAggregator {
public:
    // EWMA decay factor for volatility (0 < alpha <= 1, higher = more responsive)
    static constexpr double kDefaultEwmaAlpha = 0.05;
    static constexpr size_t kMaxMidHistory    = 200;

    explicit MarketDataAggregator(double ewma_alpha = kDefaultEwmaAlpha);

    void on_book_update(const VenueBookSnapshot& snapshot);
    InstrumentMarketView get_view(InstrumentId id) const;
    bool has_view(InstrumentId id) const;

private:
    struct InstrumentState {
        InstrumentMarketView view;
        std::deque<double>   mid_history;     // rolling window of mid prices
        double               ewma_variance = 0.0;
        bool                 initialized   = false;
    };

    void update_volatility(InstrumentState& state, double new_mid);
    void rebuild_aggregate(InstrumentState& state);

    double ewma_alpha_;
    std::unordered_map<InstrumentId, InstrumentState> states_;
};

} // namespace mme
