#include "cppallytest.h"
using namespace cppally;

// Each binary arithmetic operator is exercised across the r_int / r_int64 / r_dbl
// type combinations, with a non-NA case and an NA-on-lhs case per combination.
// Operand values are lhs = 7, rhs = 2 throughout. Result types follow common_math_t
// (except `/`, which is always r_dbl). The tail of test_arithmetic also covers
// interop with plain C++ int / double operands.

[[cppally::register]]
void test_arithmetic(){

  // ---- operator+ (7 + 2 = 9) ----
  expect_identical(r_int(7)   + r_int(2),   r_int(9));
  expect_identical(r_int(7)   + r_int64(2), r_int64(9));
  expect_identical(r_int(7)   + r_dbl(2),   r_dbl(9));
  expect_identical(r_int64(7) + r_int64(2), r_int64(9));
  expect_identical(r_int64(7) + r_dbl(2),   r_dbl(9));
  expect_identical(r_dbl(7)   + r_dbl(2),   r_dbl(9));

  expect_identical(na<r_int>()   + r_int(2),   na<r_int>());
  expect_identical(na<r_int>()   + r_int64(2), na<r_int64>());
  expect_identical(na<r_int>()   + r_dbl(2),   na<r_dbl>());
  expect_identical(na<r_int64>() + r_int64(2), na<r_int64>());
  expect_identical(na<r_int64>() + r_dbl(2),   na<r_dbl>());
  expect_identical(na<r_dbl>()   + r_dbl(2),   na<r_dbl>());

  // ---- operator- (7 - 2 = 5) ----
  expect_identical(r_int(7)   - r_int(2),   r_int(5));
  expect_identical(r_int(7)   - r_int64(2), r_int64(5));
  expect_identical(r_int(7)   - r_dbl(2),   r_dbl(5));
  expect_identical(r_int64(7) - r_int64(2), r_int64(5));
  expect_identical(r_int64(7) - r_dbl(2),   r_dbl(5));
  expect_identical(r_dbl(7)   - r_dbl(2),   r_dbl(5));

  expect_identical(na<r_int>()   - r_int(2),   na<r_int>());
  expect_identical(na<r_int>()   - r_int64(2), na<r_int64>());
  expect_identical(na<r_int>()   - r_dbl(2),   na<r_dbl>());
  expect_identical(na<r_int64>() - r_int64(2), na<r_int64>());
  expect_identical(na<r_int64>() - r_dbl(2),   na<r_dbl>());
  expect_identical(na<r_dbl>()   - r_dbl(2),   na<r_dbl>());

  // ---- operator* (7 * 2 = 14) ----
  expect_identical(r_int(7)   * r_int(2),   r_int(14));
  expect_identical(r_int(7)   * r_int64(2), r_int64(14));
  expect_identical(r_int(7)   * r_dbl(2),   r_dbl(14));
  expect_identical(r_int64(7) * r_int64(2), r_int64(14));
  expect_identical(r_int64(7) * r_dbl(2),   r_dbl(14));
  expect_identical(r_dbl(7)   * r_dbl(2),   r_dbl(14));

  expect_identical(na<r_int>()   * r_int(2),   na<r_int>());
  expect_identical(na<r_int>()   * r_int64(2), na<r_int64>());
  expect_identical(na<r_int>()   * r_dbl(2),   na<r_dbl>());
  expect_identical(na<r_int64>() * r_int64(2), na<r_int64>());
  expect_identical(na<r_int64>() * r_dbl(2),   na<r_dbl>());
  expect_identical(na<r_dbl>()   * r_dbl(2),   na<r_dbl>());

  // ---- operator/ (7 / 2 = 3.5, always r_dbl) ----
  expect_identical(r_int(7)   / r_int(2),   r_dbl(3.5));
  expect_identical(r_int(7)   / r_int64(2), r_dbl(3.5));
  expect_identical(r_int(7)   / r_dbl(2),   r_dbl(3.5));
  expect_identical(r_int64(7) / r_int64(2), r_dbl(3.5));
  expect_identical(r_int64(7) / r_dbl(2),   r_dbl(3.5));
  expect_identical(r_dbl(7)   / r_dbl(2),   r_dbl(3.5));

  expect_identical(na<r_int>()   / r_int(2),   na<r_dbl>());
  expect_identical(na<r_int>()   / r_int64(2), na<r_dbl>());
  expect_identical(na<r_int>()   / r_dbl(2),   na<r_dbl>());
  expect_identical(na<r_int64>() / r_int64(2), na<r_dbl>());
  expect_identical(na<r_int64>() / r_dbl(2),   na<r_dbl>());
  expect_identical(na<r_dbl>()   / r_dbl(2),   na<r_dbl>());

  // ---- operator% (7 %% 2 = 1, floored) ----
  expect_identical(r_int(7)   % r_int(2),   r_int(1));
  expect_identical(r_int(7)   % r_int64(2), r_int64(1));
  expect_identical(r_int(7)   % r_dbl(2),   r_dbl(1));
  expect_identical(r_int64(7) % r_int64(2), r_int64(1));
  expect_identical(r_int64(7) % r_dbl(2),   r_dbl(1));
  expect_identical(r_dbl(7)   % r_dbl(2),   r_dbl(1));

  expect_identical(na<r_int>()   % r_int(2),   na<r_int>());
  expect_identical(na<r_int>()   % r_int64(2), na<r_int64>());
  expect_identical(na<r_int>()   % r_dbl(2),   na<r_dbl>());
  expect_identical(na<r_int64>() % r_int64(2), na<r_int64>());
  expect_identical(na<r_int64>() % r_dbl(2),   na<r_dbl>());
  expect_identical(na<r_dbl>()   % r_dbl(2),   na<r_dbl>());

  // ---- operator+= (result keeps lhs type; mixed-width narrows back) ----
  { r_int   x(7); x += r_int(2);   expect_identical(x, r_int(9));   }
  { r_int   x(7); x += r_int64(2); expect_identical(x, r_int(9));   }
  { r_int   x(7); x += r_dbl(2);   expect_identical(x, r_int(9));   }
  { r_int64 x(7); x += r_int64(2); expect_identical(x, r_int64(9)); }
  { r_int64 x(7); x += r_dbl(2);   expect_identical(x, r_int64(9)); }
  { r_dbl   x(7); x += r_dbl(2);   expect_identical(x, r_dbl(9));   }

  { r_int   x = na<r_int>();   x += r_int(2);   expect_identical(x, na<r_int>());   }
  { r_int   x = na<r_int>();   x += r_int64(2); expect_identical(x, na<r_int>());   }
  { r_int   x = na<r_int>();   x += r_dbl(2);   expect_identical(x, na<r_int>());   }
  { r_int64 x = na<r_int64>(); x += r_int64(2); expect_identical(x, na<r_int64>()); }
  { r_int64 x = na<r_int64>(); x += r_dbl(2);   expect_identical(x, na<r_int64>()); }
  { r_dbl   x = na<r_dbl>();   x += r_dbl(2);   expect_identical(x, na<r_dbl>());   }

  // ---- operator-= ----
  { r_int   x(7); x -= r_int(2);   expect_identical(x, r_int(5));   }
  { r_int   x(7); x -= r_int64(2); expect_identical(x, r_int(5));   }
  { r_int   x(7); x -= r_dbl(2);   expect_identical(x, r_int(5));   }
  { r_int64 x(7); x -= r_int64(2); expect_identical(x, r_int64(5)); }
  { r_int64 x(7); x -= r_dbl(2);   expect_identical(x, r_int64(5)); }
  { r_dbl   x(7); x -= r_dbl(2);   expect_identical(x, r_dbl(5));   }

  { r_int   x = na<r_int>();   x -= r_int(2);   expect_identical(x, na<r_int>());   }
  { r_int   x = na<r_int>();   x -= r_int64(2); expect_identical(x, na<r_int>());   }
  { r_int   x = na<r_int>();   x -= r_dbl(2);   expect_identical(x, na<r_int>());   }
  { r_int64 x = na<r_int64>(); x -= r_int64(2); expect_identical(x, na<r_int64>()); }
  { r_int64 x = na<r_int64>(); x -= r_dbl(2);   expect_identical(x, na<r_int64>()); }
  { r_dbl   x = na<r_dbl>();   x -= r_dbl(2);   expect_identical(x, na<r_dbl>());   }

  // ---- operator*= ----
  { r_int   x(7); x *= r_int(2);   expect_identical(x, r_int(14));   }
  { r_int   x(7); x *= r_int64(2); expect_identical(x, r_int(14));   }
  { r_int   x(7); x *= r_dbl(2);   expect_identical(x, r_int(14));   }
  { r_int64 x(7); x *= r_int64(2); expect_identical(x, r_int64(14)); }
  { r_int64 x(7); x *= r_dbl(2);   expect_identical(x, r_int64(14)); }
  { r_dbl   x(7); x *= r_dbl(2);   expect_identical(x, r_dbl(14));   }

  { r_int   x = na<r_int>();   x *= r_int(2);   expect_identical(x, na<r_int>());   }
  { r_int   x = na<r_int>();   x *= r_int64(2); expect_identical(x, na<r_int>());   }
  { r_int   x = na<r_int>();   x *= r_dbl(2);   expect_identical(x, na<r_int>());   }
  { r_int64 x = na<r_int64>(); x *= r_int64(2); expect_identical(x, na<r_int64>()); }
  { r_int64 x = na<r_int64>(); x *= r_dbl(2);   expect_identical(x, na<r_int64>()); }
  { r_dbl   x = na<r_dbl>();   x *= r_dbl(2);   expect_identical(x, na<r_dbl>());   }

  // ---- operator/= (integer lhs floors like %/%; a float divisor floors then narrows to lhs type) ----
  { r_int   x(7); x /= r_int(2);   expect_identical(x, r_int(3));    }
  { r_int   x(7); x /= r_int64(2); expect_identical(x, r_int(3));    }
  { r_int   x(7); x /= r_dbl(2);   expect_identical(x, r_int(3));    }  // 7 / 2 = 3.5 -> floor 3
  { r_int64 x(7); x /= r_int64(2); expect_identical(x, r_int64(3));  }
  { r_int64 x(7); x /= r_dbl(2);   expect_identical(x, r_int64(3));  }
  { r_dbl   x(7); x /= r_int(2);   expect_identical(x, r_dbl(3.5));  }
  { r_dbl   x(7); x /= r_int64(2); expect_identical(x, r_dbl(3.5));  }
  { r_dbl   x(7); x /= r_dbl(2);   expect_identical(x, r_dbl(3.5));  }

  { r_int   x = na<r_int>();   x /= r_int(2);   expect_identical(x, na<r_int>());   }
  { r_int   x = na<r_int>();   x /= r_int64(2); expect_identical(x, na<r_int>());   }
  { r_int   x = na<r_int>();   x /= r_dbl(2);   expect_identical(x, na<r_int>());   }
  { r_int64 x = na<r_int64>(); x /= r_int64(2); expect_identical(x, na<r_int64>()); }
  { r_int64 x = na<r_int64>(); x /= r_dbl(2);   expect_identical(x, na<r_int64>()); }
  { r_dbl   x = na<r_dbl>();   x /= r_int(2);   expect_identical(x, na<r_dbl>());   }
  { r_dbl   x = na<r_dbl>();   x /= r_int64(2); expect_identical(x, na<r_dbl>());   }
  { r_dbl   x = na<r_dbl>();   x /= r_dbl(2);   expect_identical(x, na<r_dbl>());   }

  // ---- operator%= ----
  { r_int   x(7); x %= r_int(2);   expect_identical(x, r_int(1));   }
  { r_int   x(7); x %= r_int64(2); expect_identical(x, r_int(1));   }
  { r_int   x(7); x %= r_dbl(2);   expect_identical(x, r_int(1));   }
  { r_int64 x(7); x %= r_int64(2); expect_identical(x, r_int64(1)); }
  { r_int64 x(7); x %= r_dbl(2);   expect_identical(x, r_int64(1)); }
  { r_dbl   x(7); x %= r_dbl(2);   expect_identical(x, r_dbl(1));   }

  { r_int   x = na<r_int>();   x %= r_int(2);   expect_identical(x, na<r_int>());   }
  { r_int   x = na<r_int>();   x %= r_int64(2); expect_identical(x, na<r_int>());   }
  { r_int   x = na<r_int>();   x %= r_dbl(2);   expect_identical(x, na<r_int>());   }
  { r_int64 x = na<r_int64>(); x %= r_int64(2); expect_identical(x, na<r_int64>()); }
  { r_int64 x = na<r_int64>(); x %= r_dbl(2);   expect_identical(x, na<r_int64>()); }
  { r_dbl   x = na<r_dbl>();   x %= r_dbl(2);   expect_identical(x, na<r_dbl>());   }

  // ---- plain C++ scalars interop with cppally types ----
  // Per binary operator: plain int lhs, plain int rhs, plain double lhs, plain double rhs.
  // The templated cppally operator is the exact match, so it wins over the built-in via
  // r_int/r_dbl's implicit conversion; result type follows common_math_t.
  expect_identical(int(7)    + r_int(2),   r_int(9));
  expect_identical(r_int(7)  + int(2),     r_int(9));
  expect_identical(double(7) + r_dbl(2),   r_dbl(9));
  expect_identical(r_dbl(7)  + double(2),  r_dbl(9));

  expect_identical(int(7)    - r_int(2),   r_int(5));
  expect_identical(r_int(7)  - int(2),     r_int(5));
  expect_identical(double(7) - r_dbl(2),   r_dbl(5));
  expect_identical(r_dbl(7)  - double(2),  r_dbl(5));

  expect_identical(int(7)    * r_int(2),   r_int(14));
  expect_identical(r_int(7)  * int(2),     r_int(14));
  expect_identical(double(7) * r_dbl(2),   r_dbl(14));
  expect_identical(r_dbl(7)  * double(2),  r_dbl(14));

  expect_identical(int(7)    / r_int(2),   r_dbl(3.5)); // `/` is always r_dbl
  expect_identical(r_int(7)  / int(2),     r_dbl(3.5));
  expect_identical(double(7) / r_dbl(2),   r_dbl(3.5));
  expect_identical(r_dbl(7)  / double(2),  r_dbl(3.5));

  expect_identical(int(7)    % r_int(2),   r_int(1));
  expect_identical(r_int(7)  % int(2),     r_int(1));
  expect_identical(double(7) % r_dbl(2),   r_dbl(1));
  expect_identical(r_dbl(7)  % double(2),  r_dbl(1));

  // In-place operators take a plain C++ right-hand side (the lhs must be a cppally lvalue)
  { r_int x(7); x += 2;   expect_identical(x, r_int(9));   }
  { r_dbl x(7); x += 2.0; expect_identical(x, r_dbl(9));   }
  { r_int x(7); x -= 2;   expect_identical(x, r_int(5));   }
  { r_dbl x(7); x -= 2.0; expect_identical(x, r_dbl(5));   }
  { r_int x(7); x *= 2;   expect_identical(x, r_int(14));  }
  { r_dbl x(7); x *= 2.0; expect_identical(x, r_dbl(14));  }
  { r_int x(7); x /= 2;   expect_identical(x, r_int(3));   }  // floored
  { r_dbl x(7); x /= 2.0; expect_identical(x, r_dbl(3.5)); }
  { r_int x(7); x %= 2;   expect_identical(x, r_int(1));   }
  { r_dbl x(7); x %= 2.0; expect_identical(x, r_dbl(1));   }
}

[[cppally::register]]
void test_overflow(){

  // ---- addition: past the max wraps to NA ----
  expect_identical(r_limits<r_int>::max()   + r_int(1),   na<r_int>());
  expect_identical(r_limits<r_int64>::max() + r_int64(1), na<r_int64>());
  // one below max is still representable (no false positive)
  expect_identical((r_limits<r_int>::max() - r_int(1)) + r_int(1), r_limits<r_int>::max());

  // ---- subtraction: past the min wraps to NA ----
  expect_identical(r_limits<r_int>::min()   - r_int(2),   na<r_int>());
  expect_identical(r_limits<r_int64>::min() - r_int64(2), na<r_int64>());
  // min - 1 lands exactly on INT_MIN, which is the NA sentinel
  expect_identical(r_limits<r_int>::min() - r_int(1), na<r_int>());

  // ---- multiplication ----
  expect_identical(r_int(46341) * r_int(46341), na<r_int>());      // 2147488281 > INT_MAX
  expect_identical(r_int(46340) * r_int(46340), r_int(2147395600)); // no overflow
  expect_identical(r_int64(3037000500LL) * r_int64(3037000500LL), na<r_int64>());

  // ---- compound assignment and increment/decrement inherit the checks ----
  { r_int x = r_limits<r_int>::max(); x += r_int(1); expect_identical(x, na<r_int>()); }
  { r_int x(46341);                   x *= r_int(46341); expect_identical(x, na<r_int>()); }
  { r_int x = r_limits<r_int>::max(); ++x; expect_identical(x, na<r_int>()); }
  { r_int x = r_limits<r_int>::min(); --x; expect_identical(x, na<r_int>()); }
}

[[cppally::register]]
void test_arithmetic_edge_cases(){

  // ---- division by zero: integer -> NA, float -> NaN (distinct from NA) ----
  expect_identical(r_int(7)   % r_int(0),   na<r_int>());
  expect_identical(r_int64(7) % r_int64(0), na<r_int64>());
  { r_int   x(7); x /= r_int(0);   expect_identical(x, na<r_int>());   }
  { r_int64 x(7); x /= r_int64(0); expect_identical(x, na<r_int64>()); }

  expect_identical(r_dbl(7) % r_dbl(0), r_dbl(R_NaN));
  expect_identical(r_dbl(7) / r_dbl(0), r_dbl(R_PosInf));
  { r_dbl x(7); x /= r_dbl(0); expect_identical(x, r_dbl(R_PosInf)); }

  // ---- floored division: sign follows the divisor (R's %% and %/%), not C ----
  expect_identical(r_int(-7)   % r_int(2),    r_int(1));    // -7 %% 2  == 1
  expect_identical(r_int(7)    % r_int(-2),   r_int(-1));   //  7 %% -2 == -1
  expect_identical(r_int64(-7) % r_int64(2),  r_int64(1));
  expect_identical(r_dbl(-7)   % r_dbl(2),    r_dbl(1));    // float path floors too

  { r_int x(-7); x /= r_int(2);  expect_identical(x, r_int(-4)); }  // -7 %/% 2  == -4
  { r_int x(7);  x /= r_int(-2); expect_identical(x, r_int(-4)); }  //  7 %/% -2 == -4
  { r_int x(-7); x /= r_dbl(2.5); expect_identical(x, r_int(-3)); } // floor(-2.8) == -3, not trunc -2

  // ---- large r_int64 division stays in the integer domain (no double round-trip) ----
  // 2^53 + 1 is not representable as a double; /= 1 must leave it untouched
  { r_int64 x(9007199254740993LL); x /= r_int64(1); expect_identical(x, r_int64(9007199254740993LL)); }
}
