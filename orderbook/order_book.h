#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <list>
#include <unordered_map>
#include <iostream>
#include <iomanip>
#include <memory_resource>
#include <array>
#include <memory>

struct Order {
    uint64_t order_id;
    bool is_buy;
    double price;
    uint64_t quantity;
    uint64_t timestamp_ns;
};

struct PriceLevel {
    double price;
    uint64_t total_quantity;
};

struct InternalPriceLevel {
    uint64_t total_quantity = 0;
    std::pmr::list<Order> orders;
    InternalPriceLevel(std::pmr::memory_resource* pool) : orders(pool) {}
};

struct OrderLocation {
    double price;
    bool is_buy;
    std::pmr::list<Order>::iterator iterator;
};

class OrderBook {
public:
    OrderBook();
    void add_order(const Order& order);
    bool cancel_order(uint64_t order_id);
    bool amend_order(uint64_t order_id, double new_price, uint64_t new_quantity);
    void get_snapshot(size_t depth, std::vector<PriceLevel>& bids, std::vector<PriceLevel>& asks) const;
    void print_book(size_t depth = 10) const;

private:
    void remove_order_internal(uint64_t order_id, const OrderLocation& location);
    void try_match();

    std::unique_ptr<std::array<std::byte, 10 * 1024 * 1024>> memory_buffer_ptr_;
    std::pmr::monotonic_buffer_resource memory_pool_;
    
    using PriceMap = std::pmr::map<double, InternalPriceLevel, std::greater<double>>;
    PriceMap bids_;

    using AskMap = std::pmr::map<double, InternalPriceLevel, std::less<double>>;
    AskMap asks_;

    using LookupMap = std::pmr::unordered_map<uint64_t, OrderLocation>;
    LookupMap order_lookup_;
};