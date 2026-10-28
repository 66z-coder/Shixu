#pragma once
#include <string>
// Legacy C matching input. amount is GRAMS, never count units.
// Unknown-weight batches cannot be represented here; use V1 InventorySnapshot
// and B's availability evaluation for the new workflow. This is not a DB batch ID.
struct FoodBatch {
    std::string foodId;   // Legacy opaque identifier; C does not interpret it.
    std::string foodName; // A's canonical food name, UTF-8.
    double amount = 0.0;  // Known usable grams; <=0 ignored by legacy C matching.
};
