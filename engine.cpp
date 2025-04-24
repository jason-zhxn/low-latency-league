#include "engine.hpp"
#include <functional>
#include <optional>
#include <stdexcept>
#include <memory>




inline __attribute__((always_inline, hot)) void set_bit(std::array<uint64_t,NUM_WORDS>& bm, int& idx)   {
   bm[idx>>6] |=  1ULL << (idx & 63); }
inline __attribute__((always_inline,hot)) void clear_bit(std::array<uint64_t,NUM_WORDS>& bm, int& idx) { 
  bm[idx>>6] &= ~(1ULL << (idx & 63)); }

inline __attribute__((always_inline,hot)) int next_filled(const std::array<uint64_t,NUM_WORDS>& bm, int& start, int& max)
{
    unsigned int w = start >> 6;
    uint64_t word = bm[w] & (~0ULL << (start & 63));   // mask bits < start

    while (true) {
        if (word)                         // at least one bit set
            return (w << 6) + __builtin_ctzll(word);
        ++w;                              // advance to next word
        int idx = w << 6;
        if (idx > max || w >= 16) return max + 1;   // sentinel “none”
        word = bm[w];                   // examine next 64‑level chunk
    }
}


// Templated helper to process matching orders.
// The Condition predicate takes the price level and the incoming order price
// and returns whether the level qualifies.
template <typename Condition>
inline __attribute__((always_inline, hot)) uint32_t process_buy_orders(const Order &order, Orderbook& ob, Condition cond, QuantityType& orderQuantity, int& minIndex, int& maxIndex) {

  auto &meta  = ob.sellBitmap;          // we consume sells
  int idx    = next_filled(meta, minIndex, maxIndex);

  uint32_t matchCount = 0;
  while (idx <= maxIndex && orderQuantity && cond(idx + MIN_PRICE, order.price)){
    auto &priceLevel = ob.sellOrders[idx];
    auto& ordersList = priceLevel.orders;

    for (long unsigned int index = priceLevel.index ;index < ordersList.size() && orderQuantity > 0;index++) {
      Order& currOrder = ob.orders[ordersList[index]];
      QuantityType trade = std::min(orderQuantity, currOrder.quantity);
      orderQuantity -= trade;
      priceLevel.volume -= trade;
      currOrder.quantity -= trade;

      ++matchCount;
      if (currOrder.quantity == 0)
        priceLevel.index++;
    }

    if (priceLevel.volume==0){   
      minIndex = idx + 1;                       // advance window
      clear_bit(meta,idx);     // we consume sells
      idx = next_filled(meta, minIndex, maxIndex);
    }
    else{
      return matchCount;
    }
    
  }
  
  return matchCount;
}


template < typename Condition>
inline __attribute__((always_inline,hot)) uint32_t process_sell_orders(const Order &order, Orderbook& ob, Condition cond, QuantityType& orderQuantity, int& minIndex, int& maxIndex) {
  auto &meta  = ob.buyBitmap;          
  int idx     = next_filled(meta, minIndex, maxIndex);

  uint32_t matchCount = 0;
  while (idx <= maxIndex && orderQuantity && cond(PRICE_RANGE + MIN_PRICE - idx, order.price)){

    auto &priceLevel= ob.buyOrders[idx];
    auto& ordersList = priceLevel.orders;

    for (long unsigned int index = priceLevel.index ;index < ordersList.size() && orderQuantity > 0;index++) {
      Order& currOrder = ob.orders[ordersList[index]];
      QuantityType trade = std::min(orderQuantity, currOrder.quantity);
      orderQuantity -= trade;
      priceLevel.volume -= trade;
      currOrder.quantity -= trade;
      ++matchCount;
      if (currOrder.quantity == 0)
        priceLevel.index++;
    }

    if (priceLevel.volume==0){  
      minIndex = idx+1;
      clear_bit(meta,idx);     
      idx     = next_filled(meta, minIndex, maxIndex);
    }
    else{
      return matchCount;
    }
    
  }
  
  return matchCount;
}

uint32_t match_order(Orderbook &orderbook, const Order &incoming) {
  uint32_t matchCount = 0;
  QuantityType quantity= incoming.quantity;
   // Create a copy to modify the quantity
  if (incoming.side == Side::BUY) {
    // For a BUY, match with sell orders priced at or below the order's price.
    matchCount = process_buy_orders(incoming, orderbook, std::less_equal<>(), quantity, orderbook.minSellIndex, orderbook.maxSellIndex);
    if (quantity > 0){
      // Order* order = new Order(incoming);
      Order &order = orderbook.orders[incoming.id];
      order.side  = Side::BUY;
      order.price = incoming.price;
      

      order.quantity = quantity;
      int index = PRICE_RANGE - (order.price-MIN_PRICE);
      orderbook.buyOrders[index].orders.emplace_back(incoming.id);
      orderbook.buyOrders[index].volume += quantity;
      set_bit(orderbook.buyBitmap,index);
      orderbook.minBuyIndex = std::min(orderbook.minBuyIndex, index);
      orderbook.maxBuyIndex = std::max(orderbook.maxBuyIndex, index);
    }
  } 
  else { // Side::SELL
    matchCount = process_sell_orders(incoming, orderbook, std::greater_equal<>(), quantity, orderbook.minBuyIndex, orderbook.maxBuyIndex);
    if (quantity > 0){
      // Order* order = new Order(incoming);
      Order& order = orderbook.orders[incoming.id];
      order.price = incoming.price;
      order.side = Side::SELL;
      order.quantity = quantity;
      int index = order.price-MIN_PRICE;
      orderbook.sellOrders[index].orders.emplace_back(incoming.id);
      orderbook.sellOrders[index].volume += quantity;
      set_bit(orderbook.sellBitmap,index);

      orderbook.minSellIndex = std::min(orderbook.minSellIndex, index);
      orderbook.maxSellIndex = std::max(orderbook.maxSellIndex, index);
      
    }
      
  }
  return matchCount;
}


void modify_order_by_id(Orderbook &orderbook, IdType order_id,
                        QuantityType new_quantity) {

  // might have to check for existence
  if (order_id<orderbook.orders.size() && orderbook.orders[order_id].quantity>0){
    
    int q = orderbook.orders[order_id].quantity;
    if (orderbook.orders[order_id].side == Side::BUY) {
      int index = PRICE_RANGE - (orderbook.orders[order_id].price - MIN_PRICE);
      orderbook.buyOrders[index].volume -= (q-new_quantity);
    } else {
      orderbook.sellOrders[orderbook.orders[order_id].price - MIN_PRICE].volume -= (q-new_quantity);
    } 
    orderbook.orders[order_id].quantity = new_quantity;
  }

}


uint32_t get_volume_at_level(Orderbook &ob, Side side,
                             PriceType quantity) {


    if (side == Side::BUY) {
      return ob.buyOrders[PRICE_RANGE - (quantity-MIN_PRICE)].volume;
    }


    return ob.sellOrders[quantity-MIN_PRICE].volume;
    

}

// Functions below here don't need to be performant. Just make sure they're
// correct
Order lookup_order_by_id(Orderbook &orderbook, IdType order_id) {

  if (order_id<orderbook.orders.size() && orderbook.orders[order_id].quantity>0){
    return orderbook.orders[order_id];
  }

  throw std::runtime_error("Order not found");
}

bool order_exists(Orderbook &orderbook, IdType order_id) {
    
  if (order_id<orderbook.orders.size() &&  orderbook.orders[order_id].quantity>0){
    return true;
  }
  return false;
}

Orderbook *create_orderbook() { return new Orderbook; }
