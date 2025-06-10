#include "strategy/market_maker_controller.hpp"

namespace mme {

MarketMakerController::MarketMakerController(
    MarketDataAggregator& md,
    RiskManager& risk,
    QuoteEngine& qe,
    VenueRouter& router,
    IExecutionGateway& gw,
    std::vector<InstrumentId> instruments)
    : md_(md), risk_(risk), qe_(qe), router_(router), gw_(gw) {
    for (auto id : instruments) {
        state_[id] = InstrumentState{.id = id};
    }
}

void MarketMakerController::on_market_data(const VenueBookSnapshot& snapshot) {
    md_.on_book_update(snapshot);
    try_requote(snapshot.instrument);
}

void MarketMakerController::on_fill(InstrumentId id, VenueId /*venue*/,
                                     double price, double qty) {
    risk_.on_fill(id, price, qty);
}

void MarketMakerController::try_requote(InstrumentId id) {
    auto state_it = state_.find(id);
    if (state_it == state_.end()) return;

    auto& inst_state = state_it->second;

    if (!md_.has_view(id)) return;

    auto view = md_.get_view(id);
    if (view.mid_price <= 0.0) return;

    // Choose venue
    const auto& pos = risk_.position(id);
    VenueId venue = router_.choose_venue(view, pos);

    // Check if we should re-quote
    if (!risk_.can_quote(id, 0.1, 0.1)) return;

    // Compute quote
    Quote quote = qe_.compute_quote(view, pos, venue);
    if (quote.bid_price <= 0.0 || quote.ask_price <= 0.0) return;
    if (quote.bid_size <= 0.0 && quote.ask_size <= 0.0) return;

    // Cancel existing orders
    if (inst_state.last_bid_order_id != 0) {
        gw_.cancel_order(inst_state.last_bid_order_id);
        inst_state.last_bid_order_id = 0;
    }
    if (inst_state.last_ask_order_id != 0) {
        gw_.cancel_order(inst_state.last_ask_order_id);
        inst_state.last_ask_order_id = 0;
    }

    // Send new orders
    if (quote.bid_size > 0.0 && risk_.within_limits(id, quote.bid_size)) {
        LiveOrder bid_order{
            .id         = 0,
            .instrument = id,
            .venue      = venue,
            .side       = OrderSide::Buy,
            .price      = quote.bid_price,
            .size       = quote.bid_size,
        };
        inst_state.last_bid_order_id = gw_.send_limit_order(bid_order);
    }

    if (quote.ask_size > 0.0 && risk_.within_limits(id, -quote.ask_size)) {
        LiveOrder ask_order{
            .id         = 0,
            .instrument = id,
            .venue      = venue,
            .side       = OrderSide::Sell,
            .price      = quote.ask_price,
            .size       = quote.ask_size,
        };
        inst_state.last_ask_order_id = gw_.send_limit_order(ask_order);
    }

    inst_state.last_quote_ts = current_time_;
}

} // namespace mme
