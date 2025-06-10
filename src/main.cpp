#include "backtest/backtest_runner.hpp"
#include "config/instrument_config.hpp"
#include "config/venue_config.hpp"
#include "strategy/market_making_params.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdlib>

namespace {

// Minimal JSON value parser for config loading (no external deps)
struct JsonValue {
    enum Type { Null, Number, String, Array, Object };
    Type type = Null;
    double number = 0;
    std::string str;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;

    double get_number(const std::string& key, double def = 0) const {
        for (const auto& [k, v] : obj) {
            if (k == key && v.type == Number) return v.number;
        }
        return def;
    }
    std::string get_string(const std::string& key, const std::string& def = "") const {
        for (const auto& [k, v] : obj) {
            if (k == key && v.type == String) return v.str;
        }
        return def;
    }
    const JsonValue* get_array(const std::string& key) const {
        for (const auto& [k, v] : obj) {
            if (k == key && v.type == Array) return &v;
        }
        return nullptr;
    }
    const JsonValue* get_object(const std::string& key) const {
        for (const auto& [k, v] : obj) {
            if (k == key && v.type == Object) return &v;
        }
        return nullptr;
    }
};

// Simple recursive descent JSON parser
class JsonParser {
public:
    explicit JsonParser(const std::string& input) : input_(input), pos_(0) {}

    JsonValue parse() {
        skip_ws();
        return parse_value();
    }

private:
    std::string input_;
    size_t pos_;

    char peek() const { return pos_ < input_.size() ? input_[pos_] : '\0'; }
    char next() { return pos_ < input_.size() ? input_[pos_++] : '\0'; }
    void skip_ws() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) ++pos_;
    }

    JsonValue parse_value() {
        skip_ws();
        char c = peek();
        if (c == '"') return parse_string();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == 'n') { pos_ += 4; return JsonValue{}; }
        if (c == 't') { pos_ += 4; JsonValue v; v.type = JsonValue::Number; v.number = 1; return v; }
        if (c == 'f') { pos_ += 5; JsonValue v; v.type = JsonValue::Number; v.number = 0; return v; }
        return parse_number();
    }

    JsonValue parse_string() {
        next(); // skip opening "
        JsonValue v;
        v.type = JsonValue::String;
        while (peek() != '"' && peek() != '\0') {
            if (peek() == '\\') { next(); v.str += next(); }
            else v.str += next();
        }
        next(); // skip closing "
        return v;
    }

    JsonValue parse_number() {
        size_t start = pos_;
        if (peek() == '-') next();
        while (std::isdigit(static_cast<unsigned char>(peek()))) next();
        if (peek() == '.') { next(); while (std::isdigit(static_cast<unsigned char>(peek()))) next(); }
        if (peek() == 'e' || peek() == 'E') {
            next();
            if (peek() == '+' || peek() == '-') next();
            while (std::isdigit(static_cast<unsigned char>(peek()))) next();
        }
        JsonValue v;
        v.type = JsonValue::Number;
        v.number = std::stod(input_.substr(start, pos_ - start));
        return v;
    }

    JsonValue parse_array() {
        next(); // [
        JsonValue v;
        v.type = JsonValue::Array;
        skip_ws();
        if (peek() == ']') { next(); return v; }
        while (true) {
            v.arr.push_back(parse_value());
            skip_ws();
            if (peek() == ',') { next(); skip_ws(); }
            else break;
        }
        skip_ws();
        next(); // ]
        return v;
    }

    JsonValue parse_object() {
        next(); // {
        JsonValue v;
        v.type = JsonValue::Object;
        skip_ws();
        if (peek() == '}') { next(); return v; }
        while (true) {
            auto key = parse_string();
            skip_ws();
            next(); // :
            auto val = parse_value();
            v.obj.emplace_back(key.str, std::move(val));
            skip_ws();
            if (peek() == ',') { next(); skip_ws(); }
            else break;
        }
        skip_ws();
        next(); // }
        return v;
    }
};

mme::BacktestConfig load_config(const std::string& path) {
    std::ifstream f(path);
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

    JsonParser parser(content);
    auto root = parser.parse();

    mme::BacktestConfig config;

    // Parse instruments
    if (auto* insts = root.get_array("instruments")) {
        for (const auto& inst : insts->arr) {
            mme::InstrumentConfig ic;
            ic.id = static_cast<mme::InstrumentId>(inst.get_number("id"));
            ic.symbol = inst.get_string("symbol");
            ic.tick_size = inst.get_number("tick_size", 0.01);
            ic.lot_size = inst.get_number("lot_size", 1.0);
            ic.base_spread_bp = inst.get_number("base_spread_bp", 10.0);
            ic.inventory_limit = inst.get_number("inventory_limit", 100.0);
            config.instruments.push_back(ic);

            // Build params from instrument config + defaults
            mme::MarketMakingParams params;
            if (auto* p = inst.get_object("params")) {
                params.base_spread_bp = p->get_number("base_spread_bp", ic.base_spread_bp);
                params.min_spread_bp = p->get_number("min_spread_bp", 2.0);
                params.max_spread_bp = p->get_number("max_spread_bp", 50.0);
                params.volatility_coeff = p->get_number("volatility_coeff", 1.0);
                params.inventory_coeff = p->get_number("inventory_coeff", 0.5);
                params.size_base = p->get_number("size_base", 1.0);
                params.size_inventory_scale = p->get_number("size_inventory_scale", 0.5);
                params.quote_refresh_ms = p->get_number("quote_refresh_ms", 100.0);
                params.max_position = p->get_number("max_position", ic.inventory_limit);
            } else {
                params.base_spread_bp = ic.base_spread_bp;
                params.max_position = ic.inventory_limit;
            }
            config.params[ic.id] = params;
        }
    }

    // Parse venues
    if (auto* vens = root.get_array("venues")) {
        for (const auto& ven : vens->arr) {
            mme::VenueConfig vc;
            vc.id = static_cast<mme::VenueId>(ven.get_number("id"));
            vc.name = ven.get_string("name");
            vc.maker_fee_bp = ven.get_number("maker_fee_bp", 1.0);
            vc.taker_fee_bp = ven.get_number("taker_fee_bp", 2.0);
            vc.latency_ms = ven.get_number("latency_ms", 1.0);
            vc.cancel_penalty_bp = ven.get_number("cancel_penalty_bp", 0.1);
            config.venues.push_back(vc);
        }
    }

    config.data_file = root.get_string("data_file");
    config.fill_probability = root.get_number("fill_probability", 0.3);

    return config;
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    std::string config_path = "data/config.json";
    bool synthetic = true;
    size_t num_ticks = 10000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--ticks" && i + 1 < argc) {
            num_ticks = std::stoull(argv[++i]);
        } else if (arg == "--data") {
            synthetic = false;
        } else if (arg == "--help") {
            std::cout << "Usage: market_maker [options]\n"
                      << "  --config <path>  Config file (default: data/config.json)\n"
                      << "  --ticks <n>      Number of synthetic ticks (default: 10000)\n"
                      << "  --data           Use CSV data from config instead of synthetic\n"
                      << "  --help           Show this help\n";
            return 0;
        }
    }

    std::cout << "Loading config from: " << config_path << "\n";
    auto config = load_config(config_path);

    mme::BacktestRunner runner(config);

    if (synthetic) {
        std::cout << "Running synthetic backtest with " << num_ticks << " ticks, "
                  << config.params.size() << " instruments, "
                  << config.venues.size() << " venues...\n";
        runner.run_synthetic(num_ticks, config.instruments.size(), config.venues.size());
    } else {
        std::cout << "Running backtest from data file: " << config.data_file << "\n";
        runner.run();
    }

    // Output results
    runner.write_report("REPORT.md");
    runner.write_csv("data/backtest_results.csv");

    std::cout << "\n" << runner.metrics().generate_report();
    std::cout << "\nResults written to REPORT.md and data/backtest_results.csv\n";

    return 0;
}
