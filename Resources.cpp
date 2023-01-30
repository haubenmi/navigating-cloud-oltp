#include "Resources.hpp"
#include "infra/Config.hpp"
#include "infra/Parser.hpp"
#include "infra/Math.hpp"
#include <cassert>
#include <iomanip>
#include <sstream>
#include <string_view>
//--------------------------------------------------------------------------------
using namespace std;
using namespace infra;
//--------------------------------------------------------------------------------
Timeunit Price::timeunitForPrint = Timeunit::Hour;
bool Price::machineReadable = false;
bool Latency::machineReadable = false;
//--------------------------------------------------------------------------------
Latency Latency::fix() const {
  Latency result = *this;
  if (result.avg < result.min) result.avg = result.min;
  if (result.max < result.avg) result.max = result.avg;
  return result;
}
//--------------------------------------------------------------------------------
Latency Latency::combine(initializer_list<pair<double,Latency>> weights) {

  Latency result;
  //  assert(accumulate(weights.begin(), weights.end(), [](auto& w, double t) { return t + w.first; }, 0.0) == 1.0);
  result.min = nanoseconds(100h);
  result.max = nanoseconds(0ns);
  double weightSum = 0;
  duration<double> avg{0.0};
  for (auto& w : weights) {
    weightSum += w.first;
    if (w.first != 0) {
       result.min = std::min(result.min, w.second.min);
       result.max = std::max(result.max, w.second.max);
       avg += w.first * w.second.avg;
    }
  }
  assert((weightSum >= 0.9999) && (weightSum <= 1.00001));
  result.avg = duration_cast<nanoseconds>(avg);
  return result;
}
//--------------------------------------------------------------------------------
Latency Latency::deduce(Latency target, initializer_list<pair<double, Latency>> weights) {

  // Example target is 40us, and you already have 0.2 * 20us and 0.1 * 80us
  // then you still have 0.7 weight left, and want to calc (40 - 0.2 * 20 - 0.1 * 80) / 0.7 = 28 / 0.7 = 40

  double leftWeight = 1.0;
  duration<double> avg = target.avg;
  for (auto& w : weights) {
    if (target.avg < (w.first * w.second.avg)) {
       avg = duration<double>(0.0);
    } else {
       avg -= w.first * w.second.avg;
    }
    leftWeight -= w.first;
  }
  if (leftWeight == 0.0) return Latency::infinite();
  return Latency{duration_cast<nanoseconds>(avg / leftWeight)};
}
//--------------------------------------------------------------------------------
double Latency::getRatio(Latency target, Latency lower, Latency higher) {
  assert(lower.avg < higher.avg);
  if (target.avg >= higher.avg) return 0.0;
  if (target.avg <= lower.avg) return 1.0;
  double result = (1.0 * (higher.avg - target.avg)) / (higher.avg - lower.avg);
  assert(result >= 0.0 && result <= 1.0);
  return result;
}
//--------------------------------------------------------------------------------
static ostream& printTimestampWithUnit(ostream& out, chrono::nanoseconds val) {
   static constexpr string_view units[] = {"ns","us","ms","s"};

   uint64_t temp = val.count();
   unsigned orders = 0;
   while (temp >= 1000) {
      temp /= 1000;
      ++orders;
   }
   if (orders > 3) { orders = 3; }

   uint64_t finalDivisor = 1;
   for(unsigned i = 0; i < orders; ++i) finalDivisor *= 1000;

   double result = (double)val.count()/finalDivisor;
   assert(orders <= 6);
   stringstream ss;
   int64_t result_int = result;
   if (result_int == result) {
      ss << result_int;
   } else {
      ss << std::fixed << std::setprecision(1) << result;
   }
   ss << units[orders];
   out << ss.str();
   return out;
}
//--------------------------------------------------------------------------------
ostream& operator<<(ostream& out, const Latency& p) {
  if (Latency::machineReadable) {
    out << p.avg.count();
  } else if(Latency::verbose) {
     printTimestampWithUnit(out, p.min);
     //  out << chrono::duration_cast<chrono::milliseconds>(p.min).count();
     out << "-";
     printTimestampWithUnit(out, p.avg);
     out << "-";
     printTimestampWithUnit(out, p.max);
  } else {
     printTimestampWithUnit(out, p.avg);
  }
  return out;
}
//--------------------------------------------------------------------------------
string EBS::getDescription() const {
  stringstream ss;
  ss << numDevices << "x";
  ss << getTypeName(type);
  ss << "(";
  BinaryUnitInterpreter::print(ss, size);
  ss << "b;";
  DecimalUnitInterpreter::print(ss, iops);
  ss << "op/s;";
  DecimalUnitInterpreter::print(ss, throughput);
  ss << "b/s)";
  return ss.str();
}
//--------------------------------------------------------------------------------
bool FailoverTime::machineReadable = false;
//--------------------------------------------------------------------------------
ostream& operator<<(ostream& out, const FailoverTime& r) {
  stringstream ss;
  ss << r.value;
  if (!FailoverTime::machineReadable) {
     ss << "s";
  }
  out << ss.str();
  return out;
}
//--------------------------------------------------------------------------------
bool Durability::machineReadable = false;
//--------------------------------------------------------------------------------
ostream& operator<<(ostream& out, const Durability& r) {
  stringstream ss;
  ss << fixed << setprecision(18) << r.numericValue;
  auto res = ss.str();
  if (r.numericValue == 1.0) {
    //    out << "100%";
    out << "20";
    return out;
  }
  assert(r.numericValue < 1.0);
  unsigned nines = 0;
  for (unsigned i = 2; i < res.size(); ++i) {
    if (res.data()[i] == '9') ++nines;
    else break;
  }
  stringstream result;
  result << nines;
  if (!Durability::machineReadable) {
    result << "x9's";
  }
  // for (unsigned i = 2; i < res.size(); ++i) {
  //    char x = res.data()[i];
  //    if (i == 2) {
  //       result << x;
  //    } else if (i == 3) {
  //       result << x << ".";
  //    } else if (x != '9') {
  //       result << x;
  //       break;
  //    } else {
  //       result << x;
  //    }
  // }
  // result << "%";
  out << result.str();
  return out;
}
//--------------------------------------------------------------------------------
Durability EBS::getDurability(Type type) {
   switch (type) {
      case Type::gp3: return Durability(gp3_durability);
      case Type::gp2: return Durability(gp2_durability);
      case Type::io2: return Durability(io2_durability);
      case Type::io1: return Durability(io1_durability);
      case Type::io2x: return Durability(io2x_durability);
   }
   unreachable();
}
//--------------------------------------------------------------------------------
string EBS::getTypeName(Type t) {
   switch (t) {
      case Type::gp3: return "gp3";
      case Type::gp2: return "gp2";
      case Type::io2: return "io2";
      case Type::io1: return "io1";
      case Type::io2x: return "io2x";
   }
   unreachable();
}
//--------------------------------------------------------------------------------
Price EBS::getPrice() const { return numDevices * getSingleVolumePrice(); }
//--------------------------------------------------------------------------------
Price EBS::getSingleVolumePrice() const {

  switch (type) {
     case Type::gp3: {
       auto price = divRoundUp(size, 1_gib) * gp3_storagePerGB;
       if (iops> gp3_free_iops) {
         price += (iops-gp3_free_iops) * gp3_iop;
       }
       if (throughput > gp3_free_throughput) {
         price += divRoundUp(throughput - gp3_free_throughput, 1_mib) * gp3_throughput;
       }
       return price;
     }
     case Type::gp2: return divRoundUp(size, 1_gib) * gp2_storagePerGB;
     case Type::io2:
     case Type::io2x: {
        auto price = divRoundUp(size, 1_gib) * io_storagePerGB;
        auto firstCategoryIops = min(iops, uint64_t(32000));
        auto rest = iops - firstCategoryIops;
        auto secondCategoryIops = min(rest, uint64_t(32000));
        auto thirdCategoryIops = rest - secondCategoryIops;
        price += firstCategoryIops * io_iop;
        price += secondCategoryIops * io2_iopsAfter32k;
        price += thirdCategoryIops * iox_iopsAfter64k;
        return price;
        }
     case Type::io1: {
        auto price = divRoundUp(size, 1_gib) * io_storagePerGB;
        return price + iops * io_iop;
     }
     default: return Price::zero;
  }
}
//--------------------------------------------------------------------------------
EBS EBS::createVolume(string_view instanceName, Type type, uint64_t capacity, uint64_t iops, uint64_t throughput, uint64_t iopSize) {
   if (instanceName.starts_with("r5b") && (type == EBS::Type::io2)) {
      type = EBS::Type::io2x;
   }

   if (iopSize > maxIopSize) iopSize = maxIopSize;
   iops = max(iops, throughput / iopSize);
   uint64_t numDevices = 1;

   const auto& constraints = ([](Type t) {
      switch (t) {
         case Type::gp3: return gp3;
         case Type::gp2: return gp2;
         case Type::io2: return io2;
         case Type::io2x: return io2x;
         case Type::io1: return io1;
      }
      unreachable();
   }) (type);

   // Increase capacity if needed for Iops
   capacity = max(capacity, divRoundUp(iops, constraints.maxIopsPerGB) * 1_gib);

   auto reqDevForCap = divRoundUp(capacity, constraints.maxCapacity);
   auto reqDevForIops = divRoundUp(iops, constraints.maxIops);
   auto reqDevForThrough = divRoundUp(throughput, constraints.maxThroughput);
   numDevices = vmax(reqDevForCap, reqDevForIops, reqDevForThrough);
   if (numDevices == 0) return EBS{0, 0, 0, Type::io2, 0};

   capacity = divRoundUp(capacity, numDevices);
   iops = divRoundUp(iops, numDevices);
   throughput = divRoundUp(throughput, numDevices);

   capacity = max(capacity, constraints.minCapacity);
   capacity = max(capacity, divRoundUp(iops, constraints.maxIopsPerGB) * 1_gib);

   iops = max(iops, constraints.minIops);

   assert(constraints.minCapacity <= capacity && capacity <= constraints.maxCapacity);
   assert(constraints.minIops <= iops && iops <= constraints.maxIops);
   assert(constraints.minThroughput <= throughput && throughput <= constraints.maxThroughput);
   assert(iops <= max(constraints.minIops, (capacity / 1_gib) * constraints.maxIopsPerGB));

   return EBS{capacity, iops, throughput, type, numDevices};
}
//--------------------------------------------------------------------------------
string EBSAllotment::describe() const {
  stringstream ss;
  ss << EBS::getTypeName(type);
  ss << "(";
  BinaryUnitInterpreter::print(ss, size);
  ss << "b;";
  DecimalUnitInterpreter::print(ss, iops.rate);
  ss << "op/s;";
  DecimalUnitInterpreter::print(ss, bandwidth);
  ss << "b/s)";
  return ss.str();
}
//--------------------------------------------------------------------------------
string InstanceStorage::storageTypeToString() const {
   switch (type) {
      case InstanceStorage::Type::NVMe: return "nvme";
      case InstanceStorage::Type::SSD: return "ssd";
      case InstanceStorage::Type::HDD: return "hdd";
      case InstanceStorage::Type::None: return "none";
   }
   unreachable();
}
//--------------------------------------------------------------------------------
string InstanceStorage::getDescription() const {
  stringstream ss;
  if (devices != 1.0) {
     ss << devices << "x";
  }
  BinaryUnitInterpreter::print(ss, size);
  assert(type != Type::None);
  ss << "b(";
  ss << storageTypeToString();
  ss << ";";
  DecimalUnitInterpreter::print(ss, readOps);
  ss << " r/s;";
  DecimalUnitInterpreter::print(ss, writeOps);
  ss << " w/s)";
  return ss.str();
}
//--------------------------------------------------------------------------------
static pair<double, string> forTimeframe(double v) {
   string suffix = "h";
   if (Price::timeunitForPrint == Timeunit::Day) {
      v *= 24;
      suffix = "d";
   } else if (Price::timeunitForPrint == Timeunit::Month) {
      v *= (24 * 30);
      suffix = "m";
   } else if (Price::timeunitForPrint == Timeunit::Year) {
      v *= (24 * 365);
      suffix = "y";
   } else if (Price::timeunitForPrint == Timeunit::Minute) {
      v /= 60;
      suffix = "min";
   } else if (Price::timeunitForPrint == Timeunit::Second) {
      v /= 3600;
      suffix = "s";
   }
   return make_pair(v,suffix);
}
//--------------------------------------------------------------------------------
ostream& operator<<(ostream& out, const Price& p) {
   if (Price::machineReadable) {
      out << p.value;
      return out;
   }
   stringstream ss;
   ss << fixed << setprecision(1);

   if (p.bill == Price::Bill::PerHour) {
      auto [price, unit] = forTimeframe(p.value);
      ss << price << "$/" << unit;
   } else {
      ss << p.value << "$/1000 Req";
   }
   out << ss.str();
   return out;
}
//--------------------------------------------------------------------------------
ostream& operator<<(ostream& out, const Rate& r) {
   if (Price::machineReadable) {
      out << r.rate;
      return out;
   }
   stringstream ss;
   ss << fixed << setprecision(1) << r.rate;
   ss << "/s";
   out << ss.str();
   return out;
}
//--------------------------------------------------------------------------------
Price operator*(double mul, Price p) {
   p.value *= mul;
   return p;
}
//--------------------------------------------------------------------------------
Price operator*(Price p, Rate r) {
   assert(p.bill == Price::Bill::PerRequest);
   p.value *= 3600 * r.rate; // Rate is normalized per second, but we want to output price per hour
   p.bill = Price::Bill::PerHour;
   return p;
}
//--------------------------------------------------------------------------------
Rate Parameter::getLogWritesRequiredForUpdates(uint64_t maxIopSize) const {
   return requiredUpdateOps * (groupCommit ? (getLogRecordSize() * 1.0) / maxIopSize : divRoundUp(getLogRecordSize(), maxIopSize));
}
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
