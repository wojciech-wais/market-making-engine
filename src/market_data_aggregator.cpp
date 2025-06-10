#include "market/market_data_aggregator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace mme {

MarketDataAggregator::MarketDataAggregator(double ewma_alpha)
    : ewma_alpha_(ewma_alpha) {}

void MarketDataAggregator::on_book_update(const VenueBookSnapshot& snapshot) {
    auto& state = states_[snapshot.instrument];
    state.view.id = snapshot.instrument;

    // Update or add venue snapshot
    bool found = false;
    for (auto& vs : state.view.venues) {
        if (vs.venue == snapshot.venue) {
            vs = snapshot;
            found = true;
            break;
        }
    }
    if (!found) {
        state.view.venues.push_back(snapshot);
    }

    rebuild_aggregate(state);

    if (state.view.mid_price > 0.0) {
        update_volatility(state, state.view.mid_price);
    }
}

InstrumentMarketView MarketDataAggregator::get_view(InstrumentId id) const {
    auto it = states_.find(id);
    if (it == states_.end()) {
        return InstrumentMarketView{.id = id};
    }
    return it->second.view;
}

bool MarketDataAggregator::has_view(InstrumentId id) const {
    return states_.count(id) > 0;
}

void MarketDataAggregator::rebuild_aggregate(InstrumentState& state) {
    double global_best_bid = 0.0;
    double global_best_ask = std::numeric_limits<double>::max();
    double total_depth = 0.0;

    for (const auto& vs : state.view.venues) {
        if (!vs.bids.empty()) {
            global_best_bid = std::max(global_best_bid, vs.best_bid());
        }
        if (!vs.asks.empty()) {
            global_best_ask = std::min(global_best_ask, vs.best_ask());
        }

        // Weighted depth: sum of top-3 levels' quantity across venues
        for (size_t i = 0; i < std::min(vs.bids.size(), size_t(3)); ++i) {
            total_depth += vs.bids[i].quantity;
        }
        for (size_t i = 0; i < std::min(vs.asks.size(), size_t(3)); ++i) {
            total_depth += vs.asks[i].quantity;
        }
    }

    if (global_best_bid > 0.0 && global_best_ask < std::numeric_limits<double>::max()) {
        state.view.mid_price = (global_best_bid + global_best_ask) / 2.0;
        state.view.spread = global_best_ask - global_best_bid;
    }

    state.view.weighted_depth = total_depth;
    state.view.volatility = std::sqrt(state.ewma_variance);
}

void MarketDataAggregator::update_volatility(InstrumentState& state, double new_mid) {
    state.mid_history.push_back(new_mid);
    if (state.mid_history.size() > kMaxMidHistory) {
        state.mid_history.pop_front();
    }

    if (state.mid_history.size() < 2) {
        return;
    }

    // Compute log return
    double prev = state.mid_history[state.mid_history.size() - 2];
    if (prev <= 0.0) return;

    double log_return = std::log(new_mid / prev);

    if (!state.initialized) {
        state.ewma_variance = log_return * log_return;
        state.initialized = true;
    } else {
        // EWMA: variance_t = alpha * r_t^2 + (1 - alpha) * variance_{t-1}
        state.ewma_variance = ewma_alpha_ * (log_return * log_return)
                            + (1.0 - ewma_alpha_) * state.ewma_variance;
    }

    state.view.volatility = std::sqrt(state.ewma_variance);
}

} // namespace mme
