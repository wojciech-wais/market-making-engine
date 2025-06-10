#pragma once

namespace mme {

struct MarketMakingParams {
    double base_spread_bp       = 10.0;   // baseline target spread in bp of mid
    double min_spread_bp        = 2.0;    // floor
    double max_spread_bp        = 50.0;   // cap
    double volatility_coeff     = 1.0;    // how much to widen spread with vol
    double inventory_coeff      = 0.5;    // how much to skew based on inventory
    double size_base            = 1.0;    // base quote size
    double size_inventory_scale = 0.5;    // scale size vs inventory (beta)
    double quote_refresh_ms     = 100.0;  // min time between re-quotes
    double max_position         = 100.0;  // absolute position limit
};

} // namespace mme
