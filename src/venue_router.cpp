#include "execution/venue_router.hpp"

#include <algorithm>
#include <limits>

namespace mme {

VenueRouter::VenueRouter(std::vector<VenueConfig> venues)
    : venues_(std::move(venues)) {}

VenueId VenueRouter::choose_venue(const InstrumentMarketView& view,
                                   const InstrumentPosition& /*pos*/) const {
    if (venues_.empty()) {
        return 0;
    }

    // Score each venue: lower is better.
    // Effective cost = maker_fee + latency_penalty + cancel_penalty - depth_bonus
    VenueId best_venue = venues_.front().id;
    double best_score = std::numeric_limits<double>::max();

    for (const auto& vc : venues_) {
        double score = vc.maker_fee_bp + vc.cancel_penalty_bp + vc.latency_ms * 0.01;

        // Prefer venues with more depth for this instrument
        for (const auto& vs : view.venues) {
            if (vs.venue == vc.id) {
                double venue_depth = 0.0;
                for (const auto& lvl : vs.bids) venue_depth += lvl.quantity;
                for (const auto& lvl : vs.asks) venue_depth += lvl.quantity;
                // Depth bonus: more depth = lower score
                score -= venue_depth * 0.001;
                break;
            }
        }

        if (score < best_score) {
            best_score = score;
            best_venue = vc.id;
        }
    }

    return best_venue;
}

} // namespace mme
