#pragma once
#include "infra/CSV.hpp"
#include <chrono>
#include <numeric>
#include <cmath>
#include <type_traits>
#include <utility>
//--------------------------------------------------------------------------------
using namespace infra;
using namespace std::chrono;
//--------------------------------------------------------------------------------
auto constexpr operator "" _kib(unsigned long long x) -> uint64_t { return 1024L * x; }
auto constexpr operator "" _mib(unsigned long long x) -> uint64_t { return 1024L * 1024L * x; }
auto constexpr operator "" _gib(unsigned long long x) -> uint64_t { return 1024L * 1024L * 1024L * x; }
auto constexpr operator "" _tib(unsigned long long x) -> uint64_t { return 1024L * 1024L * 1024L * 1024L * x; }
//--------------------------------------------------------------------------------
template<typename T>
T vmin(T&&t)
{
  return std::forward<T>(t);
}
//--------------------------------------------------------------------------------
template<typename T0, typename T1, typename... Ts>
typename std::common_type<
  T0, T1, Ts...
>::type vmin(T0&& val1, T1&& val2, Ts&&... vs)
{
  if (val2 < val1)
    return vmin(val2, std::forward<Ts>(vs)...);
  else
    return vmin(val1, std::forward<Ts>(vs)...);
}
//--------------------------------------------------------------------------------
template<typename T>
T vmax(T&&t)
{
  return std::forward<T>(t);
}
//--------------------------------------------------------------------------------
template<typename T0, typename T1, typename... Ts>
typename std::common_type<
  T0, T1, Ts...
>::type vmax(T0&& val1, T1&& val2, Ts&&... vs)
{
  if (val2 > val1)
    return vmax(val2, std::forward<Ts>(vs)...);
  else
    return vmax(val1, std::forward<Ts>(vs)...);
}
//--------------------------------------------------------------------------------
template<typename... Ts>
typename std::common_type<Ts...>::type vmaxafter(Ts&&... vs) {
  return std::nextafter(vmax(std::forward<Ts>(vs)...), std::numeric_limits<typename std::common_type<Ts...>::type>::max());
}
//--------------------------------------------------------------------------------
template<typename T>
inline void hash_combine(std::size_t& seed, const T& v)
{
   std::hash<T> hasher;
   seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}
//--------------------------------------------------------------------------------
template <typename... Ts>
inline size_t multihash(const Ts& ...ts) {
  size_t result = 0x1824812;
  (hash_combine(result, ts), ...);
  return result;
}
//--------------------------------------------------------------------------------
struct VantageSchema : public CSVSchema {
  using CSVSchema::CSVSchema;
   CSVBool consider{this, "consider_instance"};
   CSVBool previousGeneration{this, "previous_generation"};

   CSVString name{this, "name"};
   CSVString id{this, "id"};
   CSVNumber cpu{this, "vcpu"};
   CSVString cpuVendor{this, "cpu_vendor"};
   CSVNumber memory{this, "memory"};
   CSVNumber clock{this, "clock_speed_ghz"};
   CSVNumber price{this, "price"};
   CSVString priceUnit{this, "price_unit"};

   CSVBool network_upto{this, "network_upto"};
   CSVNumber network_speed{this, "network_speed"};
   CSVNumber network_speed_burst{this, "network_speed_burst"};
   CSVNumber network_devices{this, "network_devices"};

   CSVBool storage_nvme{this, "nvme_ssd"};
   CSVBool storage_ssd{this, "storage_ssd"};
   CSVNumber storage_devices{this, "storage_devices"};
   CSVNumber storage_size{this, "storage_size"};
   CSVNumber storage_readops{this, "storage_readops"};
   CSVNumber storage_writeops{this, "storage_writeops"};

   CSVNumber ebs_base_iops{this, "ebs_base_iops"};
   CSVNumber ebs_burst_iops{this, "ebs_burst_iops"};
   CSVNumber ebs_base_throughput{this, "ebs_base_throughput"};
   CSVNumber ebs_burst_throughput{this, "ebs_burst_throughput"};
};
//--------------------------------------------------------------------------------
struct VantageCSV : public CSV<VantageSchema> {
};
//--------------------------------------------------------------------------------
