#pragma once

#include "execution/execution_gateway.hpp"
#include "market/market_view.hpp"

#include <unordered_map>
#include <functional>
#include <cstdint>

namespace mme {

// Callback invoked when a simulated fill occurs.
// Parameters: instrument_id, venue_id, price, signed_qty (+ for buy, - for sell)
using FillCallback = std::function<void(InstrumentId, VenueId, double, double)>;

class SimExecutionGateway : public IExecutionGateway {
public:
    explicit SimExecutionGateway(FillCallback on_fill);

    uint64_t send_limit_order(const LiveOrder& order) override;
    void     cancel_order(uint64_t order_id) override;

    // Drive the simulation: check resting orders against current book snapshot.
    // Fills occur if the order price crosses the opposite side of the book.
    void check_fills(const VenueBookSnapshot& snapshot);

    size_t active_order_count() const { return orders_.size(); }

private:
    uint64_t next_order_id_ = 1;
    std::unordered_map<uint64_t, LiveOrder> orders_;
    FillCallback on_fill_;
};

class NullExecutionGateway : public IExecutionGateway {
public:
    uint64_t send_limit_order(const LiveOrder& order) override;
    void     cancel_order(uint64_t order_id) override;

    uint64_t orders_sent()      const { return orders_sent_; }
    uint64_t cancels_sent()     const { return cancels_sent_; }

private:
    uint64_t next_order_id_ = 1;
    uint64_t orders_sent_   = 0;
    uint64_t cancels_sent_  = 0;
};

} // namespace mme
