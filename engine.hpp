#pragma once

#include <cstdint>
#include <list>
#include <map>
#include <vector>
#include <unordered_map>
#include <memory>

enum class Side : uint8_t { BUY, SELL };

using IdType = uint32_t;
using PriceType = uint16_t;
using QuantityType = uint16_t;

// You CANNOT change this
struct Order {
  IdType id; // Unique
  PriceType price;
  QuantityType quantity;
  Side side;
};

struct PriceLevel{
  long unsigned int index;
  std::vector<std::shared_ptr<Order>> orders;
  int volume;
};

// You CAN and SHOULD change this
struct Orderbook {
  IdType firstId;
  std::map<PriceType, PriceLevel, std::greater<PriceType>> buyOrders;
  std::map<PriceType, PriceLevel> sellOrders;
  std::vector<std::shared_ptr<Order>> orders;
  // std::unordered_map<IdType, std::shared_ptr<Order>> orders;
};

extern "C" {

// Takes in an incoming order, matches it, and returns the number of matches
// Partial fills are valid

uint32_t match_order(Orderbook &orderbook, const Order &incoming);

// Sets the new quantity of an order. If new_quantity==0, removes the order
void modify_order_by_id(Orderbook &orderbook, IdType order_id,
                        QuantityType new_quantity);

// Returns total resting volume at a given price point
uint32_t get_volume_at_level(Orderbook &orderbook, Side side,
                             PriceType quantity);

// Performance of these do not matter. They are only used to check correctness
Order lookup_order_by_id(Orderbook &orderbook, IdType order_id);
bool order_exists(Orderbook &orderbook, IdType order_id);
Orderbook *create_orderbook();
}
