#include "order_book.h"
#include <algorithm>
#include <vector>

OrderBook::OrderBook()
    : memory_buffer_ptr_(std::make_unique<std::array<std::byte, 10 * 1024 * 1024>>()),
      memory_pool_(memory_buffer_ptr_->data(), memory_buffer_ptr_->size()),
      bids_(std::greater<double>(), &memory_pool_),
      asks_(std::less<double>(), &memory_pool_),
      order_lookup_(&memory_pool_)
{}

void OrderBook::add_order(const Order& order) {
    if (order_lookup_.count(order.order_id)) {
        return;
    }

    if (order.is_buy) {
        auto [level_it, _] = bids_.try_emplace(order.price, &memory_pool_);
        InternalPriceLevel& level = level_it->second;

        level.orders.push_back(order);
        level.total_quantity += order.quantity;

        auto list_it = std::prev(level.orders.end());
        order_lookup_[order.order_id] = {order.price, order.is_buy, list_it};
    } else {
        auto [level_it, _] = asks_.try_emplace(order.price, &memory_pool_);
        InternalPriceLevel& level = level_it->second;

        level.orders.push_back(order);
        level.total_quantity += order.quantity;

        auto list_it = std::prev(level.orders.end());
        order_lookup_[order.order_id] = {order.price, order.is_buy, list_it};
    }

    try_match();
}

bool OrderBook::cancel_order(uint64_t order_id) {
    auto lookup_it = order_lookup_.find(order_id);
    if (lookup_it == order_lookup_.end()) {
        return false;
    }

    remove_order_internal(order_id, lookup_it->second);
    return true;
}

bool OrderBook::amend_order(uint64_t order_id, double new_price, uint64_t new_quantity) {
    auto lookup_it = order_lookup_.find(order_id);
    if (lookup_it == order_lookup_.end()) {
        return false;
    }

    const OrderLocation& location = lookup_it->second;
    Order& order = *(location.iterator);

    if (order.price != new_price) {
        Order new_order = order;
        new_order.price = new_price;
        new_order.quantity = new_quantity;

        remove_order_internal(order_id, location);
        add_order(new_order);
    } else if (order.quantity != new_quantity) {
        int64_t qty_diff = static_cast<int64_t>(new_quantity) - static_cast<int64_t>(order.quantity);
        
        if (order.is_buy) {
            auto map_it = bids_.find(order.price);
            map_it->second.total_quantity = static_cast<uint64_t>(static_cast<int64_t>(map_it->second.total_quantity) + qty_diff);
        } else {
            auto map_it = asks_.find(order.price);
            map_it->second.total_quantity = static_cast<uint64_t>(static_cast<int64_t>(map_it->second.total_quantity) + qty_diff);
        }
        order.quantity = new_quantity;
    }
    
    return true;
}

void OrderBook::remove_order_internal(uint64_t order_id, const OrderLocation& location) {
    double price = location.price;
    bool is_buy = location.is_buy;
    auto list_it = location.iterator;
    uint64_t quantity = list_it->quantity;

    if (is_buy) {
        auto map_it = bids_.find(price);
        InternalPriceLevel& level = map_it->second;

        level.total_quantity -= quantity;
        level.orders.erase(list_it);

        if (level.orders.empty()) {
            bids_.erase(map_it);
        }
    } else {
        auto map_it = asks_.find(price);
        InternalPriceLevel& level = map_it->second;

        level.total_quantity -= quantity;
        level.orders.erase(list_it);

        if (level.orders.empty()) {
            asks_.erase(map_it);
        }
    }

    order_lookup_.erase(order_id);
}

void OrderBook::try_match() {
    while (!bids_.empty() && !asks_.empty() && bids_.begin()->first >= asks_.begin()->first) {
        
        auto bid_level_it = bids_.begin();
        auto ask_level_it = asks_.begin();
        InternalPriceLevel& bid_level = bid_level_it->second;
        InternalPriceLevel& ask_level = ask_level_it->second;
        
        Order& bid_order = bid_level.orders.front();
        Order& ask_order = ask_level.orders.front();

        uint64_t trade_qty = std::min(bid_order.quantity, ask_order.quantity);

        std::cout << "--- EXECUTION ---" << std::endl;
        std::cout << "  Matched " << trade_qty << " shares at " << ask_level_it->first << std::endl;
        std::cout << "  Buy Order: " << bid_order.order_id << " | Sell Order: " << ask_order.order_id << std::endl;
        std::cout << "-----------------" << std::endl;

        bid_level.total_quantity -= trade_qty;
        bid_order.quantity -= trade_qty;

        ask_level.total_quantity -= trade_qty;
        ask_order.quantity -= trade_qty;

        if (bid_order.quantity == 0) {
            order_lookup_.erase(bid_order.order_id);
            bid_level.orders.pop_front();
        }

        if (ask_order.quantity == 0) {
            order_lookup_.erase(ask_order.order_id);
            ask_level.orders.pop_front();
        }

        if (bid_level.orders.empty()) {
            bids_.erase(bid_level_it);
        }

        if (ask_level.orders.empty()) {
            asks_.erase(ask_level_it);
        }
    }
}

void OrderBook::get_snapshot(size_t depth, 
                             std::vector<PriceLevel>& bids, 
                             std::vector<PriceLevel>& asks) const 
{
    bids.clear();
    asks.clear();
    
    bids.reserve(depth);
    asks.reserve(depth);

    size_t count = 0;
    for (const auto& [price, level] : bids_) {
        if (count >= depth) break;
        bids.push_back({price, level.total_quantity});
        count++;
    }

    count = 0;
    for (const auto& [price, level] : asks_) {
        if (count >= depth) break;
        asks.push_back({price, level.total_quantity});
        count++;
    }
}

void OrderBook::print_book(size_t depth) const {
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    get_snapshot(depth, bids, asks);

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "--- ORDER BOOK ---" << std::endl;
    std::cout << "--------------------" << std::endl;
    std::cout << "|   ASKS (SELL)    |" << std::endl;
    std::cout << "--------------------" << std::endl;
    std::cout << "| Price    | Qty   |" << std::endl;
    std::cout << "--------------------" << std::endl;

    std::vector<PriceLevel> reversed_asks = asks;
    std::reverse(reversed_asks.begin(), reversed_asks.end());

    for (const auto& level : reversed_asks) {
        std::cout << "| " << std::setw(8) << level.price 
                  << " | " << std::setw(5) << level.total_quantity << " |" << std::endl;
    }

    std::cout << "--------------------" << std::endl;
    std::cout << "|    BIDS (BUY)    |" << std::endl;
    std::cout << "--------------------" << std::endl;
    std::cout << "| Price    | Qty   |" << std::endl;
    std::cout << "--------------------" << std::endl;

    for (const auto& level : bids) {
        std::cout << "| " << std::setw(8) << level.price 
                  << " | " << std::setw(5) << level.total_quantity << " |" << std::endl;
    }
    std::cout << "--------------------" << std::endl;
}