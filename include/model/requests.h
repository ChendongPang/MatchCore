#pragma once

#include "common/enums.h"
#include "common/types.h"

namespace matching_engine::model {

// New order request submitted to engine.
struct NewOrderRequest {
    OrderId order_id {kInvalidOrderId};
    SymbolId symbol_id {kInvalidSymbolId};

    Side side {Side::Buy};
    OrderType type {OrderType::Limit};

    // For market order in V1, price may be kMarketOrderPrice / 0.
    Price price {kInvalidPrice};

    Quantity quantity {kInvalidQuantity};

    // External timestamp if caller has one.
    // If caller does not care, engine may still assign internal sequence.
    Timestamp timestamp {kInvalidTimestamp};
};

// Cancel an active resting order by id.
struct CancelRequest {
    SymbolId symbol_id {kInvalidSymbolId};
    OrderId order_id {kInvalidOrderId};
    Timestamp timestamp {kInvalidTimestamp};
};

// Replace is modeled as cancel + new in V1.
// old_order_id is the order being replaced.
// new_order_id is the new order identity after replace.
struct ReplaceRequest {
    SymbolId symbol_id {kInvalidSymbolId};

    OrderId old_order_id {kInvalidOrderId};
    OrderId new_order_id {kInvalidOrderId};

    Price new_price {kInvalidPrice};
    Quantity new_quantity {kInvalidQuantity};

    Timestamp timestamp {kInvalidTimestamp};
};

} // namespace matching_engine::model