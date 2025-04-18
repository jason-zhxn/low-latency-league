#include "engine.hpp"
#include <functional>
#include <optional>
#include <stdexcept>
#include <memory>


// This is an example correct implementation
// It is INTENTIONALLY suboptimal
// You are encouraged to rewrite as much or as little as you'd like

// Templated helper to process matching orders.
// The Condition predicate takes the price level and the incoming order price
// and returns whether the level qualifies.
template <typename OrderMap, typename Condition>
uint32_t process_orders(const Order &order, OrderMap &ordersMap, Condition cond, QuantityType& orderQuantity) {
  uint32_t matchCount = 0;
  auto it = ordersMap.begin();
  while (it != ordersMap.end() && order.quantity > 0 && cond(it->first, order.price)) {
    auto &ordersAtPrice = it->second;
    auto& ordersList = ordersAtPrice.orders;
    for (long unsigned int index = ordersAtPrice.index ;index < ordersList.size() && orderQuantity > 0;index++) {
      auto currOrder = ordersList[index];
      QuantityType trade = std::min(orderQuantity, currOrder->quantity);
      orderQuantity -= trade;
      ordersAtPrice.volume -= trade;
      currOrder->quantity -= trade;
      ++matchCount;
      if (currOrder->quantity == 0)
        ordersAtPrice.index++;
      else{
        return matchCount;
      }
    }
    if (ordersAtPrice.index==ordersList.size()) //ordersAtPrice.empty())
      it = ordersMap.erase(it);
    else
      break;
  }
  return matchCount;
}

uint32_t match_order(Orderbook &orderbook, const Order &incoming) {
  uint32_t matchCount = 0;
  QuantityType quantity= incoming.quantity;
   // Create a copy to modify the quantity
  if (incoming.side == Side::BUY) {
    // For a BUY, match with sell orders priced at or below the order's price.
    matchCount = process_orders(incoming, orderbook.sellOrders, std::less_equal<>(), quantity);
    if (quantity > 0){
      Order* order = new Order(incoming);
      order->quantity = quantity;
      orderbook.buyOrders[order->price].orders.push_back(order);
      orderbook.buyOrders[order->price].volume += quantity;
      orderbook.orders[order->id]= order;
      
      

    }
  } 
  else { // Side::SELL
    // For a SELL, match with buy orders priced at or above the order's price.
    matchCount = process_orders(incoming, orderbook.buyOrders, std::greater_equal<>(), quantity);
    if (quantity > 0){
      Order* order = new Order(incoming);
      order->quantity = quantity;
      orderbook.sellOrders[order->price].orders.push_back(order);
      orderbook.sellOrders[order->price].volume += quantity;
      orderbook.orders[order->id] = order;
      
      
    }
      
  }
  return matchCount;
}


void modify_order_by_id(Orderbook &orderbook, IdType order_id,
                        QuantityType new_quantity) {

  // might have to check for existence
  if (order_id<orderbook.orders.size() && orderbook.orders[order_id]){
    
    int q = orderbook.orders[order_id]->quantity;
    if (orderbook.orders[order_id]->side == Side::BUY) {
      orderbook.buyOrders[orderbook.orders[order_id]->price].volume -= (q-new_quantity);
    } else {
      orderbook.sellOrders[orderbook.orders[order_id]->price].volume -= (q-new_quantity);
    } 
    orderbook.orders[order_id]->quantity = new_quantity;
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

uint32_t get_volume_at_level(Orderbook &orderbook, Side side,
                             PriceType quantity) {
  if (side == Side::SELL) {
    return orderbook.sellOrders[quantity].volume;
  }

  return orderbook.buyOrders[quantity].volume;

}

// Functions below here don't need to be performant. Just make sure they're
// correct
Order lookup_order_by_id(Orderbook &orderbook, IdType order_id) {
  if (order_id<orderbook.orders.size() && orderbook.orders[order_id] && orderbook.orders[order_id]->quantity>0){
    return *orderbook.orders[order_id];
  }
  throw std::runtime_error("Order not found");
}

bool order_exists(Orderbook &orderbook, IdType order_id) {
    
  if (order_id<orderbook.orders.size() && orderbook.orders[order_id] && orderbook.orders[order_id]->quantity>0){
    return true;
  }
  return false;
}

Orderbook *create_orderbook() { return new Orderbook; }
