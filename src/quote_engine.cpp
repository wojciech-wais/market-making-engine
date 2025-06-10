#include "strategy/quote_engine.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>

namespace mme {

QuoteEngine::QuoteEngine(std::unordered_map<InstrumentId, MarketMakingParams> params)
    : params_(std::move(params)) {}

Quote QuoteEngine::compute_quote(const InstrumentMarketView& view,
                                  const InstrumentPosition& position,
                                  VenueId venue) const {
    auto it = params_.find(view.id);
    if (it == params_.end()) {
        return Quote{.id = view.id, .venue = venue};
    }

    const auto& p = it->second;
    double mid = view.mid_price;

    if (mid <= 0.0) {
        return Quote{.id = view.id, .venue = venue};
    }

    double spread_bp = compute_spread(p, view.volatility);
    double spread_abs = spread_bp * mid / 10000.0;

    double skew = compute_skew(p, position.quantity, p.max_position, spread_abs);
    double size = compute_size(p, position.quantity, p.max_position);

    double bid_price = mid - spread_abs / 2.0 - skew;
    double ask_price = mid + spread_abs / 2.0 - skew;

    // Determine per-side size based on position limits
    double bid_size = size;
    double ask_size = size;

    // If close to long limit, reduce bid size; if close to short limit, reduce ask size
    double normalized_inv = (p.max_position > 0) ? position.quantity / p.max_position : 0.0;
    if (normalized_inv > 0.8) {
        bid_size *= std::max(0.1, 1.0 - normalized_inv);
    }
    if (normalized_inv < -0.8) {
        ask_size *= std::max(0.1, 1.0 + normalized_inv);
    }

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    return Quote{
        .id        = view.id,
        .venue     = venue,
        .bid_price = bid_price,
        .ask_price = ask_price,
        .bid_size  = bid_size,
        .ask_size  = ask_size,
        .ts        = static_cast<Timestamp>(now),
    };
}

double QuoteEngine::compute_spread(const MarketMakingParams& p, double volatility) const {
    // spread = max(min_spread, min(max_spread, base_spread + vol_coeff * volatility))
    // volatility is in log-return units; scale to bp by multiplying by 10000
    double vol_bp = volatility * 10000.0;
    double spread = p.base_spread_bp + p.volatility_coeff * vol_bp;
    return std::clamp(spread, p.min_spread_bp, p.max_spread_bp);
}

double QuoteEngine::compute_skew(const MarketMakingParams& p,
                                  double inventory,
                                  double max_position,
                                  double spread) const {
    if (max_position <= 0.0) return 0.0;

    // Normalized inventory: q_tilde = q / Q_max
    double q_tilde = inventory / max_position;

    // Skew delta = inventory_coeff * q_tilde * spread
    return p.inventory_coeff * q_tilde * spread;
}

double QuoteEngine::compute_size(const MarketMakingParams& p,
                                  double inventory,
                                  double max_position) const {
    if (max_position <= 0.0) return p.size_base;

    // size = size_base * (1 - beta * |q_tilde|), clamped at min
    double q_tilde = std::abs(inventory) / max_position;
    double size = p.size_base * (1.0 - p.size_inventory_scale * q_tilde);
    return std::max(size, p.size_base * 0.1); // floor at 10% of base
}

} // namespace mme
