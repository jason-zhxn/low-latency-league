#pragma once

#include <cstdint>
#include <list>
#include <vector>
#include <memory>
#include <array>

enum class Side : uint8_t { BUY, SELL };

// inline constexpr uint32_t MIN_PRICE = 3500;
inline constexpr uint32_t MIN_PRICE = 0;
inline constexpr uint32_t PRICE_RANGE = 1000;

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
  unsigned int index = 0;
  unsigned int size = 0;
  std::vector<IdType> orders{};
  unsigned int volume=0;

  PriceLevel(){
    orders.reserve(13000);
  }
};

constexpr int NUM_LEVELS = PRICE_RANGE + 1;  // 1001
constexpr int WORD_BITS  = 64;
constexpr int NUM_WORDS  = 16; // 16



// You CAN and SHOULD change this
struct Orderbook {
  // sorted highest buy to lowest buy
  std::array<PriceLevel, PRICE_RANGE+1> buyOrders{};
  // sorted lowest sell to highest sell
  std::array<PriceLevel, PRICE_RANGE+1> sellOrders{};
  std::array<uint64_t, NUM_WORDS> buyBitmap{};
  std::array<uint64_t, NUM_WORDS> sellBitmap{};

  int minSellIndex = PRICE_RANGE;
  int maxSellIndex= 0;

  int minBuyIndex = PRICE_RANGE;
  int maxBuyIndex = 0;
  
  std::array<Order, 15000> orders{};
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
