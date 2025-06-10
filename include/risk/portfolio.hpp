#pragma once

#include "config/instrument_config.hpp"

#include <unordered_map>
#include <cmath>

namespace mme {

struct InstrumentPosition {
    InstrumentId id             = 0;
    double       quantity       = 0.0;   // signed
    double       avg_price      = 0.0;   // volume-weighted
    double       realized_pnl   = 0.0;
    double       unrealized_pnl = 0.0;
};

struct PortfolioState {
    std::unordered_map<InstrumentId, InstrumentPosition> positions;
    double total_realized_pnl   = 0.0;
    double total_unrealized_pnl = 0.0;

    // sum(position * mid)
    double net_exposure(const std::unordered_map<InstrumentId, double>& mid_prices) const {
        double exposure = 0.0;
        for (const auto& [id, pos] : positions) {
            auto it = mid_prices.find(id);
            double mid = (it != mid_prices.end()) ? it->second : pos.avg_price;
            exposure += pos.quantity * mid;
        }
        return exposure;
    }

    // sum(|position| * mid)
    double gross_notional(const std::unordered_map<InstrumentId, double>& mid_prices) const {
        double notional = 0.0;
        for (const auto& [id, pos] : positions) {
            auto it = mid_prices.find(id);
            double mid = (it != mid_prices.end()) ? it->second : pos.avg_price;
            notional += std::abs(pos.quantity) * mid;
        }
        return notional;
    }
};

} // namespace mme
