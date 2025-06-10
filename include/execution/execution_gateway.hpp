#pragma once

#include "config/instrument_config.hpp"
#include "config/venue_config.hpp"

#include <cstdint>

namespace mme {

enum class OrderSide { Buy, Sell };

struct LiveOrder {
    uint64_t     id         = 0;
    InstrumentId instrument = 0;
    VenueId      venue      = 0;
    OrderSide    side       = OrderSide::Buy;
    double       price      = 0.0;
    double       size       = 0.0;
};

class IExecutionGateway {
public:
    virtual ~IExecutionGateway() = default;
    virtual uint64_t send_limit_order(const LiveOrder& order) = 0;
    virtual void     cancel_order(uint64_t order_id) = 0;
};

} // namespace mme
