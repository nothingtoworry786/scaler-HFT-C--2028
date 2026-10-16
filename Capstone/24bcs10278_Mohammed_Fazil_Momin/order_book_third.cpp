#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class Side { Buy, Sell };

struct Level {
    double price;
    long qty;
};

struct BBO {
    bool has_bid = false;
    double bid_price = 0.0;
    long bid_qty = 0;

    bool has_ask = false;
    double ask_price = 0.0;
    long ask_qty = 0;
};

class OrderBookC {
    static constexpr int MIN_BID = 5000;
    static constexpr int MAX_BID = 9999;
    static constexpr int MIN_ASK = 10001;
    static constexpr int MAX_ASK = 15000;
    static constexpr int LEVELS = 5000;

    struct Order {
        std::uint64_t id;
        Side side;
        int tick;
        long qty;
        std::uint64_t arrival_sequence;
    };

    struct PriceLevel {
        long total_qty = 0;
        std::list<Order> orders;
    };

    std::vector<PriceLevel> bids_ = std::vector<PriceLevel>(LEVELS);
    std::vector<PriceLevel> asks_ = std::vector<PriceLevel>(LEVELS);
    int best_bid_ = MIN_BID - 1;
    int best_ask_ = MAX_ASK + 1;
    std::unordered_map<std::uint64_t, std::list<Order>::iterator> lookup_;
    std::unordered_set<std::uint64_t> seen_ids_;
    std::uint64_t next_sequence_ = 0;

    PriceLevel& level(Side side, int tick) {
        return side == Side::Buy ? bids_[tick - MIN_BID] : asks_[tick - MIN_ASK];
    }
    const PriceLevel& level(Side side, int tick) const {
        return side == Side::Buy ? bids_[tick - MIN_BID] : asks_[tick - MIN_ASK];
    }

    static int to_tick(Side side, double price) {
        if (!std::isfinite(price)) return 0;
        if (side == Side::Buy && (price < 50.00 || price > 99.99)) return 0;
        if (side == Side::Sell && (price < 100.01 || price > 150.00)) return 0;
        int tick = static_cast<int>(std::lround(price * 100.0));
        if (std::abs(price * 100.0 - tick) > 1e-6) return 0;
        return tick;
    }

public:
    void add_order(std::uint64_t id, Side side, double price, long qty) {
        int tick = to_tick(side, price);
        if (tick == 0 || qty <= 0) return;
        if (!seen_ids_.insert(id).second) return;
        PriceLevel& lvl = level(side, tick);
        lvl.orders.push_back(Order{id, side, tick, qty, next_sequence_++});
        lvl.total_qty += qty;
        lookup_[id] = std::prev(lvl.orders.end());
        if (side == Side::Buy && tick > best_bid_) best_bid_ = tick;
        if (side == Side::Sell && tick < best_ask_) best_ask_ = tick;
    }

    bool cancel_order(std::uint64_t id) {
        auto it = lookup_.find(id);
        if (it == lookup_.end()) return false;
        const Order& o = *it->second;
        PriceLevel& lvl = level(o.side, o.tick);
        lvl.total_qty -= o.qty;
        lvl.orders.erase(it->second);
        lookup_.erase(it);
        while (best_bid_ >= MIN_BID && level(Side::Buy, best_bid_).total_qty == 0) --best_bid_;
        while (best_ask_ <= MAX_ASK && level(Side::Sell, best_ask_).total_qty == 0) ++best_ask_;
        return true;
    }

    BBO get_bbo() const {
        BBO bbo;
        if (best_bid_ >= MIN_BID) {
            bbo.has_bid = true;
            bbo.bid_price = best_bid_ / 100.0;
            bbo.bid_qty = level(Side::Buy, best_bid_).total_qty;
        }
        if (best_ask_ <= MAX_ASK) {
            bbo.has_ask = true;
            bbo.ask_price = best_ask_ / 100.0;
            bbo.ask_qty = level(Side::Sell, best_ask_).total_qty;
        }
        return bbo;
    }

    std::vector<Level> get_bids(int depth = 10) const {
        if (depth <= 0) return {};
        std::vector<Level> v;
        for (int t = best_bid_; t >= MIN_BID && (int)v.size() < depth; --t) {
            long qty = level(Side::Buy, t).total_qty;
            if (qty > 0) v.push_back({t / 100.0, qty});
        }
        return v;
    }

    std::vector<Level> get_asks(int depth = 10) const {
        if (depth <= 0) return {};
        std::vector<Level> v;
        for (int t = best_ask_; t <= MAX_ASK && (int)v.size() < depth; ++t) {
            long qty = level(Side::Sell, t).total_qty;
            if (qty > 0) v.push_back({t / 100.0, qty});
        }
        return v;
    }
};

int main() {
    OrderBookC ob;
    BBO e = ob.get_bbo();
    assert(!e.has_bid && !e.has_ask);
    assert(!ob.cancel_order(999));

    ob.add_order(1, Side::Buy, 99.98, 100);
    ob.add_order(2, Side::Buy, 99.98, 50);
    ob.add_order(3, Side::Buy, 99.50, 10);
    ob.add_order(4, Side::Sell, 100.01, 20);
    ob.add_order(5, Side::Sell, 100.05, 30);

    BBO b = ob.get_bbo();
    assert(b.has_bid && b.bid_price == 99.98 && b.bid_qty == 150);
    assert(b.has_ask && b.ask_price == 100.01 && b.ask_qty == 20);

    assert(ob.cancel_order(1) && !ob.cancel_order(1));
    assert(ob.get_bbo().bid_qty == 50);
    assert(ob.cancel_order(2));
    assert(ob.get_bbo().bid_price == 99.50);

    assert(ob.cancel_order(4));
    assert(ob.get_bbo().ask_price == 100.05);

    assert(ob.get_bids(10).size() == 1);
    assert(ob.get_asks(10).size() == 1);
    assert(ob.get_bids(0).empty());
    assert(ob.get_bids(-1).empty());

    assert(ob.cancel_order(3) && ob.cancel_order(5));
    assert(!ob.get_bbo().has_bid && !ob.get_bbo().has_ask);

    OrderBookC test;

    test.add_order(100, Side::Buy, 99.98, 10);
    assert(test.cancel_order(100));

    test.add_order(100, Side::Buy, 99.99, 20);
    assert(!test.cancel_order(100));
    assert(!test.get_bbo().has_bid);

    test.add_order(101, Side::Buy, 99.981, 10);
    assert(!test.get_bbo().has_bid);

    test.add_order(102, Side::Buy, 100.01, 10);
    test.add_order(103, Side::Sell, 99.99, 10);
    test.add_order(104, Side::Buy, 99.99, 0);
    test.add_order(105, Side::Buy, 49.99, 10);
    test.add_order(106, Side::Sell, 150.01, 10);
    assert(!test.get_bbo().has_bid && !test.get_bbo().has_ask);

    test.add_order(107, Side::Buy, 50.00, 1);
    test.add_order(108, Side::Sell, 150.00, 1);
    assert(test.get_bbo().bid_price == 50.00 && test.get_bbo().ask_price == 150.00);

}
