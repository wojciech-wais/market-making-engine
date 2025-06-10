#pragma once

#include <cstdint>
#include <string>

namespace mme {

using InstrumentId = uint32_t;

struct InstrumentConfig {
    InstrumentId id;
    std::string  symbol;
    double       tick_size;
    double       lot_size;
    double       base_spread_bp;      // baseline spread in basis points
    double       inventory_limit;     // max absolute position
};

} // namespace mme
