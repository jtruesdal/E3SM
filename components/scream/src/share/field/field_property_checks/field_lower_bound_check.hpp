#ifndef SCREAM_FIELD_LOWER_BOUND_CHECK_HPP
#define SCREAM_FIELD_LOWER_BOUND_CHECK_HPP

#include "share/field/field_property_checks/field_within_interval_check.hpp"

#include <limits>

namespace scream
{

// Convenience implementation of check for interval [L,+\infty). The class
// inherits from FieldWithinIntervalCheck, and sets upper bound to "infinity"

class FieldLowerBoundCheck: public FieldWithinIntervalCheck {
public:

  // No default constructor -- we need a lower bound.
  FieldLowerBoundCheck () = delete;

  // Constructor with lower and bound. By default, this property check
  // can repair fields that fail the check by overwriting nonpositive values
  // with the given lower bound. If can_repair is false, the check cannot
  // apply repairs to the field.
  explicit FieldLowerBoundCheck (const double lower_bound,
                                 bool can_repair = true) : 
    FieldWithinIntervalCheck(lower_bound, std::numeric_limits<double>::max(), can_repair)
  {
    // Do Nothing
  }

  // The name of the field check
  std::string name () const override {
    // NOTE: std::to_string does not do a good job with small numbers (like 1e-9).
    std::stringstream ss;
    ss << "Lower Bound Check of " << this->m_lower_bound;
    return ss.str();
  }

};

} // namespace scream

#endif // SCREAM_FIELD_LOWER_BOUND_CHECK_HPP
