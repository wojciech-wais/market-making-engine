#pragma once

#include "config/venue_config.hpp"
#include "market/market_view.hpp"
#include "risk/portfolio.hpp"

#include <vector>

namespace mme {

class VenueRouter {
public:
    explicit VenueRouter(std::vector<VenueConfig> venues);

    // Choose the best venue for quoting based on fees, latency, and depth.
    VenueId choose_venue(const InstrumentMarketView& view,
                         const InstrumentPosition& pos) const;

    const std::vector<VenueConfig>& venues() const { return venues_; }

private:
    std::vector<VenueConfig> venues_;
};

} // namespace mme
