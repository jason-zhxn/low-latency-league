#include "engine.hpp"
#include <functional>
#include <optional>
#include <stdexcept>
#include <memory>


// Templated helper to process matching orders.
// The Condition predicate takes the price level and the incoming order price
// and returns whether the level qualifies.
template <typename OrderMap, typename Condition>
uint32_t process_buy_orders(const Order &order, OrderMap &ordersMap, Condition cond, QuantityType& orderQuantity, int& minIndex, int& maxIndex) {
  uint32_t matchCount = 0;
  while (minIndex <= maxIndex && orderQuantity >0 && cond(minIndex + MIN_PRICE, order.price)){
    if (ordersMap[minIndex].volume==0){
      ++minIndex;
      continue; 
    }

    auto &priceLevel= ordersMap[minIndex];
    auto& ordersList = priceLevel.orders;
    // if (orderQuantity>=priceLevel.volume){
    //   orderQuantity -= priceLevel.volume;
    //   matchCount += (ordersList.size() - priceLevel.index);
    //   priceLevel.volume = 0;
    //   ++minIndex;
    //   continue;
    // }

    for (long unsigned int index = priceLevel.index ;index < ordersList.size() && orderQuantity > 0;index++) {
      auto currOrder = ordersList[index];
      QuantityType trade = std::min(orderQuantity, currOrder->quantity);
      orderQuantity -= trade;
      priceLevel.volume -= trade;
      currOrder->quantity -= trade;
      ++matchCount;
      if (currOrder->quantity == 0)
        priceLevel.index++;
    }

    if (priceLevel.volume==0){
      ++minIndex;
    }
    else{
      return matchCount;
    }
    
  }
  
  return matchCount;
}


template <typename OrderMap, typename Condition>
uint32_t process_sell_orders(const Order &order, OrderMap &ordersMap, Condition cond, QuantityType& orderQuantity, int& minIndex, int& maxIndex) {
  uint32_t matchCount = 0;
  while (minIndex <= maxIndex && orderQuantity >0 && cond(PRICE_RANGE+MIN_PRICE-minIndex, order.price)){
    if (ordersMap[minIndex].volume==0){
      ++minIndex;
      continue;
    }

    auto &priceLevel= ordersMap[minIndex];
    auto& ordersList = priceLevel.orders;
    // if (orderQuantity>=priceLevel.volume){
    //   orderQuantity -= priceLevel.volume;
    //   matchCount += (ordersList.size() - priceLevel.index);
    //   priceLevel.volume = 0;
    //   continue;
    // }

    for (long unsigned int index = priceLevel.index ;index < ordersList.size() && orderQuantity > 0;index++) {
      auto currOrder = ordersList[index];
      QuantityType trade = std::min(orderQuantity, currOrder->quantity);
      orderQuantity -= trade;
      priceLevel.volume -= trade;
      currOrder->quantity -= trade;
      ++matchCount;
      if (currOrder->quantity == 0)
        priceLevel.index++;
    }

    if (priceLevel.volume==0){
      ++minIndex;
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
    matchCount = process_buy_orders(incoming, orderbook.sellOrders, std::less_equal<>(), quantity, orderbook.minSellIndex, orderbook.maxSellIndex);
    if (quantity > 0){
      // Order* order = new Order(incoming);
      Order &order = orderbook.orders[incoming.id];
      order.side  = Side::BUY;
      order.id = incoming.id;
      order.price = incoming.price;
      

      order.quantity = quantity;
      int index = PRICE_RANGE - (order.price-MIN_PRICE);
      orderbook.buyOrders[index].orders.emplace_back(&order);
      orderbook.buyOrders[index].volume += quantity;
      orderbook.minBuyIndex = std::min(orderbook.minBuyIndex, index);
      orderbook.maxBuyIndex = std::max(orderbook.maxBuyIndex, index);
    }
  } 
  else { // Side::SELL
    matchCount = process_sell_orders(incoming, orderbook.buyOrders, std::greater_equal<>(), quantity, orderbook.minBuyIndex, orderbook.maxBuyIndex);
          // if (orderbook.sellOrders[order->price].orders.size() > 12150){
      //   std::cout << orderbook.sellOrders[order->price].orders.size() << std::endl;
      // }
    if (quantity > 0){
      // Order* order = new Order(incoming);
      Order& order = orderbook.orders[incoming.id];
      order.price = incoming.price;
      order.id = incoming.id;
      order.side = Side::SELL;
      order.quantity = quantity;

      int index = order.price-MIN_PRICE;
      orderbook.sellOrders[index].orders.emplace_back(&order);

      orderbook.sellOrders[index].volume += quantity;

      orderbook.minSellIndex = std::min(orderbook.minSellIndex, index);
      orderbook.maxSellIndex = std::max(orderbook.maxSellIndex, index);
      
    }
      
  }
  return matchCount;
}


void modify_order_by_id(Orderbook &orderbook, IdType order_id,
                        QuantityType new_quantity) {

  // might have to check for existence
  if (order_id<orderbook.orders.size()){
    
    int q = orderbook.orders[order_id].quantity;
    if (orderbook.orders[order_id].side == Side::BUY) {
      orderbook.buyOrders[PRICE_RANGE - (orderbook.orders[order_id].price - MIN_PRICE)].volume -= (q-new_quantity);
    } else {
      orderbook.sellOrders[orderbook.orders[order_id].price - MIN_PRICE].volume -= (q-new_quantity);
    } 
    orderbook.orders[order_id].quantity = new_quantity;
  }

}

template <typename OrderMap>
std::optional<Order> lookup_order_in_map(OrderMap &ordersMap, IdType order_id) {
  for (const auto &[price, priceLevel] : ordersMap) {
    auto& ordersList = priceLevel.orders;
    auto& index = priceLevel.index;
    for (unsigned long i = index; i<ordersList.size(); i++) {
      if (ordersList[i].id== order_id) {
        return ordersList[i];
      }
    }
  }
  return std::nullopt;
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
    // if (orderbook.orders[order_id].side == Side::BUY) {
    //   return orderbook.buyOrders[PRICE_RANGE - orderbook.orders[order_id].price-MIN_PRICE].volume > 0;
    // } else {
    //   return orderbook.sellOrders[orderbook.orders[order_id].price-MIN_PRICE].volume > 0;
    // }
    return true;
  }
  return false;
}

Orderbook *create_orderbook() { return new Orderbook; }
