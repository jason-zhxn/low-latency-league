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
      auto order = std::make_shared<Order>(incoming);
      order->quantity = quantity;
      if(orderbook.orders.size()==0){
        orderbook.firstId = order->id;
      }
      
      orderbook.buyOrders[order->price].orders.push_back(order);
      orderbook.orders.push_back(order);
      

    }else{
      orderbook.orders.push_back(nullptr);

    }
  } 
  else { // Side::SELL
    // For a SELL, match with buy orders priced at or above the order's price.
    matchCount = process_orders(incoming, orderbook.buyOrders, std::greater_equal<>(), quantity);
    if (quantity > 0){
      auto order = std::make_shared<Order>(incoming);
      order->quantity = quantity;
      if(orderbook.orders.size()==0){
        orderbook.firstId = order->id;
      }
      orderbook.sellOrders[order->price].orders.push_back(order);
      orderbook.orders.push_back(order);
      
    }else{
      orderbook.orders.push_back(nullptr);
    }
      
  }
  return matchCount;
}

// Templated helper to cancel an order within a given orders map.
template <typename OrderMap>
bool modify_order_in_map(OrderMap &ordersMap, IdType order_id,
                         QuantityType new_quantity) {


  for (auto it = ordersMap.begin(); it != ordersMap.end();) {
    auto &priceLevel = it->second;
    auto& ordersList = priceLevel.orders;
    for (unsigned long index = priceLevel.index; index<ordersList.size(); index++) {
      auto currOrder = ordersList[index];
      if (currOrder->id == order_id) {
        if (new_quantity == 0){
          priceLevel.index++;
          return true;
        }
        else {
          currOrder->quantity = new_quantity;
          return true;
        }
      } 
    }
    if (priceLevel.index>ordersList.size())
      it = ordersMap.erase(it);
    else
      ++it;
  }
  return false;
}

void modify_order_by_id(Orderbook &orderbook, IdType order_id,
                        QuantityType new_quantity) {

  // might have to check for existence
  uint32_t index = order_id - orderbook.firstId;
  if (index<orderbook.orders.size() && orderbook.orders[index]){
    orderbook.orders[index]->quantity = new_quantity;
  }

  // if (modify_order_in_map(orderbook.buyOrders, order_id, new_quantity))
  //   return;
  // if (modify_order_in_map(orderbook.sellOrders, order_id, new_quantity))
  //   return;
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
  uint32_t total = 0;
  if (side == Side::BUY) {
    auto buy_orders = orderbook.buyOrders.find(quantity);
    if (buy_orders == orderbook.buyOrders.end()) {
      return 0;
    }
    auto& priceLevel = buy_orders->second;
    auto& orderList = priceLevel.orders;
    for (unsigned long index = priceLevel.index; index<orderList.size(); index++) {
      total += orderList[index]->quantity;
    }
  } else if (side == Side::SELL) {
    auto sell_orders = orderbook.sellOrders.find(quantity);
    if (sell_orders == orderbook.sellOrders.end()) {
      return 0;
    }
    auto& priceLevel = sell_orders->second;
    auto& orderList = priceLevel.orders;
    for (unsigned long index = priceLevel.index; index<orderList.size(); index++) {
      total += orderList[index]->quantity;
    }
  }
  return total;
}

// Functions below here don't need to be performant. Just make sure they're
// correct
Order lookup_order_by_id(Orderbook &orderbook, IdType order_id) {
  uint32_t index = order_id - orderbook.firstId;
  if (index<orderbook.orders.size() && orderbook.orders[index] && orderbook.orders[index]->quantity>0){
    return *orderbook.orders[index];
  }
  throw std::runtime_error("Order not found");
}

bool order_exists(Orderbook &orderbook, IdType order_id) {

  
  uint32_t index = order_id - orderbook.firstId;
  if (index<orderbook.orders.size() && orderbook.orders[index] && orderbook.orders[index]->quantity>0){
    return true;
  }
  return false;
}

Orderbook *create_orderbook() { return new Orderbook; }
