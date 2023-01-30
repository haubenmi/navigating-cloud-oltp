#pragma once
//--------------------------------------------------------------------------------
#include <cstdint>
#include <cmath>
#include <type_traits>
//--------------------------------------------------------------------------------
namespace infra {
//--------------------------------------------------------------------------------
template <typename T>
static constexpr bool isPowerOf2(T t) {
   return (t & (t - 1)) == 0;
}
//--------------------------------------------------------------------------------
template <typename T>
static constexpr unsigned log2(T x) {
   return (sizeof(T) * 8 - 1) - clz(x);
}
//--------------------------------------------------------------------------------
enum class PowerOfTwo : uint8_t {};
constexpr uint8_t getShift(PowerOfTwo a) { return static_cast<std::underlying_type_t<PowerOfTwo>>(a); }
constexpr uint64_t getAlignment(PowerOfTwo a){return 1ull << getShift(a); }
//---------------------------------------------------------------------------------
constexpr uint64_t divRoundUp(uint64_t dividend, uint64_t divisor) {
  return (dividend + (divisor - 1)) / divisor;
}
//---------------------------------------------------------------------------------
constexpr uint64_t divRoundUp(double dividend, double divisor) {
   if (std::isinf(divisor)) return 0;
   auto res = dividend / divisor;
   // Cut out far decimal digits
   auto res2 = std::trunc(res * 1e6) / 1e6;
   return ceil(res2);
}
//---------------------------------------------------------------------------------
inline double roundWithPrecision(double v, unsigned precision) {
  return std::round(v * precision) / precision;
}
//---------------------------------------------------------------------------------
constexpr double getGeneralizedHarmonicNumber(uint64_t n, double m) {
  double result = 0;
  for (uint64_t k = 1; k <= n; ++k) {
    result += 1.0 / std::pow(double(k), m);
  }
  return result;
}
//---------------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------

// 0000'0000 == 0
// 0000'1000 == 8   -> log2 = 1*8-1 - clz(x) = 7 - 4 = 3
