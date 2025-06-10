#pragma once

#include <cstdint>
#include <string>

namespace mme {

using VenueId = uint8_t;

struct VenueConfig {
    VenueId      id;
    std::string  name;
    double       maker_fee_bp;
    double       taker_fee_bp;
    double       latency_ms;          // approximate venue latency
    double       cancel_penalty_bp;   // how "expensive" cancels are
};

} // namespace mme
