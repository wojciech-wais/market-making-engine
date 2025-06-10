#include "execution/sim_execution_gateway.hpp"

namespace mme {

// --- SimExecutionGateway ---

SimExecutionGateway::SimExecutionGateway(FillCallback on_fill)
    : on_fill_(std::move(on_fill)) {}

uint64_t SimExecutionGateway::send_limit_order(const LiveOrder& order) {
    uint64_t id = next_order_id_++;
    LiveOrder stored = order;
    stored.id = id;
    orders_[id] = stored;
    return id;
}

void SimExecutionGateway::cancel_order(uint64_t order_id) {
    orders_.erase(order_id);
}

void SimExecutionGateway::check_fills(const VenueBookSnapshot& snapshot) {
    std::vector<uint64_t> filled_ids;

    for (auto& [id, order] : orders_) {
        if (order.instrument != snapshot.instrument || order.venue != snapshot.venue) {
            continue;
        }

        bool fill = false;
        double fill_price = order.price;

        if (order.side == OrderSide::Buy) {
            // Buy order fills if best ask <= order price
            if (!snapshot.asks.empty() && snapshot.asks.front().price <= order.price) {
                fill = true;
                fill_price = order.price; // filled at limit
            }
        } else {
            // Sell order fills if best bid >= order price
            if (!snapshot.bids.empty() && snapshot.bids.front().price >= order.price) {
                fill = true;
                fill_price = order.price;
            }
        }

        if (fill) {
            double signed_qty = (order.side == OrderSide::Buy) ? order.size : -order.size;
            if (on_fill_) {
                on_fill_(order.instrument, order.venue, fill_price, signed_qty);
            }
            filled_ids.push_back(id);
        }
    }

    for (uint64_t id : filled_ids) {
        orders_.erase(id);
    }
}

// --- NullExecutionGateway ---

uint64_t NullExecutionGateway::send_limit_order(const LiveOrder& /*order*/) {
    ++orders_sent_;
    return next_order_id_++;
}

void NullExecutionGateway::cancel_order(uint64_t /*order_id*/) {
    ++cancels_sent_;
}

} // namespace mme
