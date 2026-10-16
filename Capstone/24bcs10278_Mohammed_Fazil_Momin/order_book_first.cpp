#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <queue>
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

class OrderBookA {
    using Tick = int;

    struct Order {
        std::uint64_t id;
        Side side;
        Tick tick;
        long qty;
    };

    struct BidCmp { bool operator()(const Order& a, const Order& b) const { return a.tick < b.tick; } };
    struct AskCmp { bool operator()(const Order& a, const Order& b) const { return a.tick > b.tick; } };

    std::unordered_map<std::uint64_t, Order> orders_;
    std::unordered_map<Tick, long> bid_levels_, ask_levels_;

    mutable std::priority_queue<Order, std::vector<Order>, BidCmp> bid_queue_;
    mutable std::priority_queue<Order, std::vector<Order>, AskCmp> ask_queue_;

    static Tick to_tick(double price) { return static_cast<Tick>(std::lround(price * 100.0)); }

    static std::vector<Level> levels(const std::unordered_map<Tick, long>& m, int depth, bool descending) {
        std::vector<Level> v;
        if (depth <= 0) return v;
        v.reserve(m.size());
        for (auto [tick, qty] : m) v.push_back({tick / 100.0, qty});
        if (descending) std::sort(v.begin(), v.end(), [](const Level& a, const Level& b) { return a.price > b.price; });
        else            std::sort(v.begin(), v.end(), [](const Level& a, const Level& b) { return a.price < b.price; });
        if (v.size() > static_cast<std::size_t>(depth)) v.resize(depth);
        return v;
    }

public:
    void add_order(std::uint64_t id, Side side, double price, long qty) {
        Order o{id, side, to_tick(price), qty};
        if (!orders_.emplace(id, o).second) return;
        if (side == Side::Buy) { bid_levels_[o.tick] += qty; bid_queue_.push(o); }
        else                   { ask_levels_[o.tick] += qty; ask_queue_.push(o); }
    }

    bool cancel_order(std::uint64_t id) {
        auto it = orders_.find(id);
        if (it == orders_.end()) return false;
        const Order& o = it->second;
        auto& lv = (o.side == Side::Buy) ? bid_levels_ : ask_levels_;
        auto l = lv.find(o.tick);
        if ((l->second -= o.qty) <= 0) lv.erase(l);
        orders_.erase(it);
        return true;
    }

    BBO get_bbo() const {
        BBO bbo;
        while (!bid_queue_.empty() && !orders_.count(bid_queue_.top().id)) bid_queue_.pop();
        if (!bid_queue_.empty()) {
            Tick t = bid_queue_.top().tick;
            bbo = {true, t / 100.0, bid_levels_.at(t), false, 0.0, 0};
        }
        while (!ask_queue_.empty() && !orders_.count(ask_queue_.top().id)) ask_queue_.pop();
        if (!ask_queue_.empty()) {
            Tick t = ask_queue_.top().tick;
            bbo.has_ask = true; bbo.ask_price = t / 100.0; bbo.ask_qty = ask_levels_.at(t);
        }
        return bbo;
    }

    std::vector<Level> get_bids(int depth = 10) const { return levels(bid_levels_, depth, true); }
    std::vector<Level> get_asks(int depth = 10) const { return levels(ask_levels_, depth, false); }
};

int main() {
    OrderBookA ob;
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
