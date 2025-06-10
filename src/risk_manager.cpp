#include "risk/risk_manager.hpp"

#include <cmath>

namespace mme {

const InstrumentPosition RiskManager::kEmptyPosition = {};

RiskManager::RiskManager(std::unordered_map<InstrumentId, MarketMakingParams> params)
    : params_(std::move(params)) {}

void RiskManager::on_fill(InstrumentId id, double price, double qty) {
    auto& pos = portfolio_.positions[id];
    pos.id = id;

    double old_qty = pos.quantity;
    double new_qty = old_qty + qty;

    // If crossing zero or increasing position
    if ((old_qty >= 0 && qty > 0) || (old_qty <= 0 && qty < 0)) {
        // Increasing position: update average price
        double total_cost = pos.avg_price * std::abs(old_qty) + price * std::abs(qty);
        pos.avg_price = (std::abs(new_qty) > 1e-12)
                            ? total_cost / std::abs(new_qty)
                            : price;
    } else {
        // Reducing position: realize P&L
        double fill_qty = std::min(std::abs(qty), std::abs(old_qty));
        double pnl = 0.0;
        if (old_qty > 0) {
            // Was long, selling
            pnl = (price - pos.avg_price) * fill_qty;
        } else {
            // Was short, buying
            pnl = (pos.avg_price - price) * fill_qty;
        }
        pos.realized_pnl += pnl;
        portfolio_.total_realized_pnl += pnl;

        // If we crossed zero, reset avg_price to the fill price for the remainder
        if (std::abs(new_qty) > 1e-12 && ((old_qty > 0 && new_qty < 0) || (old_qty < 0 && new_qty > 0))) {
            pos.avg_price = price;
        }
    }

    pos.quantity = new_qty;
}

bool RiskManager::can_quote(InstrumentId id, double bid_size, double ask_size) const {
    auto it = params_.find(id);
    if (it == params_.end()) return false;

    auto pos_it = portfolio_.positions.find(id);
    double current_qty = (pos_it != portfolio_.positions.end()) ? pos_it->second.quantity : 0.0;

    // Check if buying bid_size or selling ask_size would breach limits
    double max_pos = it->second.max_position;
    bool buy_ok = std::abs(current_qty + bid_size) <= max_pos;
    bool sell_ok = std::abs(current_qty - ask_size) <= max_pos;

    return buy_ok || sell_ok; // At least one side should be quoteable
}

bool RiskManager::within_limits(InstrumentId id, double delta_qty) const {
    auto it = params_.find(id);
    if (it == params_.end()) return false;

    auto pos_it = portfolio_.positions.find(id);
    double current_qty = (pos_it != portfolio_.positions.end()) ? pos_it->second.quantity : 0.0;

    return std::abs(current_qty + delta_qty) <= it->second.max_position;
}

void RiskManager::update_unrealized(const std::unordered_map<InstrumentId, double>& mid_prices) {
    portfolio_.total_unrealized_pnl = 0.0;
    for (auto& [id, pos] : portfolio_.positions) {
        auto it = mid_prices.find(id);
        if (it != mid_prices.end() && std::abs(pos.quantity) > 1e-12) {
            if (pos.quantity > 0) {
                pos.unrealized_pnl = (it->second - pos.avg_price) * pos.quantity;
            } else {
                pos.unrealized_pnl = (pos.avg_price - it->second) * std::abs(pos.quantity);
            }
        } else {
            pos.unrealized_pnl = 0.0;
        }
        portfolio_.total_unrealized_pnl += pos.unrealized_pnl;
    }
}

const InstrumentPosition& RiskManager::position(InstrumentId id) const {
    auto it = portfolio_.positions.find(id);
    if (it != portfolio_.positions.end()) {
        return it->second;
    }
    return kEmptyPosition;
}

} // namespace mme
