#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <set>
#include <unordered_map>
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

class OrderBookB {
    using Tick = int;

    struct Order {
        std::uint64_t id;
        Side side;
        Tick tick;
        long qty;
        std::uint64_t timestamp;
    };

    struct BidPriority {
        bool operator()(const Order& a, const Order& b) const {
            if (a.tick != b.tick) return a.tick > b.tick;
            if (a.timestamp != b.timestamp) return a.timestamp < b.timestamp;
            return a.id < b.id;
        }
    };

    struct AskPriority {
        bool operator()(const Order& a, const Order& b) const {
            if (a.tick != b.tick) return a.tick < b.tick;
            if (a.timestamp != b.timestamp) return a.timestamp < b.timestamp;
            return a.id < b.id;
        }
    };

    using BidSet = std::multiset<Order, BidPriority>;
    using AskSet = std::multiset<Order, AskPriority>;

    BidSet bids_;
    AskSet asks_;
    std::unordered_map<std::uint64_t, BidSet::iterator> bid_lookup_;
    std::unordered_map<std::uint64_t, AskSet::iterator> ask_lookup_;
    std::uint64_t next_timestamp_ = 0;

    static Tick to_tick(double price) { return static_cast<Tick>(std::lround(price * 100.0)); }

public:
    void add_order(std::uint64_t id, Side side, double price, long qty) {
        if (bid_lookup_.count(id) || ask_lookup_.count(id)) return;
        Order o{id, side, to_tick(price), qty, next_timestamp_++};
        if (side == Side::Buy) bid_lookup_[id] = bids_.insert(o);
        else                   ask_lookup_[id] = asks_.insert(o);
    }

    bool cancel_order(std::uint64_t id) {
        auto b = bid_lookup_.find(id);
        if (b != bid_lookup_.end()) {
            bids_.erase(b->second);
            bid_lookup_.erase(b);
            return true;
        }
        auto a = ask_lookup_.find(id);
        if (a != ask_lookup_.end()) {
            asks_.erase(a->second);
            ask_lookup_.erase(a);
            return true;
        }
        return false;
    }

    BBO get_bbo() const {
        BBO bbo;
        std::vector<Level> bids = get_bids(1);
        std::vector<Level> asks = get_asks(1);
        if (!bids.empty()) {
            bbo.has_bid = true;
            bbo.bid_price = bids[0].price;
            bbo.bid_qty = bids[0].qty;
        }
        if (!asks.empty()) {
            bbo.has_ask = true;
            bbo.ask_price = asks[0].price;
            bbo.ask_qty = asks[0].qty;
        }
        return bbo;
    }

    std::vector<Level> get_bids(int depth = 10) const {
        std::vector<Level> v;
        for (const Order& o : bids_) {
            double price = o.tick / 100.0;
            if (!v.empty() && v.back().price == price) {
                v.back().qty += o.qty;
            } else {
                if ((int)v.size() == depth) break;
                v.push_back({price, o.qty});
            }
        }
        return v;
    }

    std::vector<Level> get_asks(int depth = 10) const {
        std::vector<Level> v;
        for (const Order& o : asks_) {
            double price = o.tick / 100.0;
            if (!v.empty() && v.back().price == price) {
                v.back().qty += o.qty;
            } else {
                if ((int)v.size() == depth) break;
                v.push_back({price, o.qty});
            }
        }
        return v;
    }
};

int main() {
    OrderBookB ob;
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

}
