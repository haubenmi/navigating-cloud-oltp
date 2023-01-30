#pragma once
#include "Common.hpp"
#include "Metric.hpp"
#include "Architecture.hpp"
#include "infra/Terminal.hpp"
#include "infra/Math.hpp"
#include <sstream>
#include <string>
//--------------------------------------------------------------------------------
struct PrimaryMetric : public Metric {
   PrimaryMetric() : Metric{"Primary", 10} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) { out << a.getPrimary().getDescription(); }
};
//--------------------------------------------------------------------------------
struct IdMetric : public Metric {
   static uint64_t id;
   IdMetric() : Metric{"id", 3} {}
   void formatValue(std::ostream& out, const Architecture&, bool) { out << id++; }
  //  std::partial_ordering compare(const Architecture& a, const Architecture& b
};
//--------------------------------------------------------------------------------
struct SecondaryMetric : public Metric {
   SecondaryMetric() : Metric{"numSec"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getSecondaries().getCount(); }
   std::partial_ordering compare(const Architecture& a, const Architecture& b) const override { return a.getSecondaries().getCount() <=> b.getSecondaries().getCount(); }
};
//--------------------------------------------------------------------------------
struct PrimaryBufferCache : public Metric {
  PrimaryBufferCache() : Metric{"PrimCache"} {}
  void formatValue(std::ostream& out, const Architecture& a, bool raw) override { formatByte(out, a.getPrimary().getBufferCacheSize(), raw); }
};
//--------------------------------------------------------------------------------
struct PrimaryBufferCacheHitrate : public Metric {
  PrimaryBufferCacheHitrate() : Metric{"PriCaHit"} {}
  void formatValue(std::ostream& out, const Architecture& a, bool /*raw*/) override { out << a.getPrimary().probCacheHit(); }
};
//--------------------------------------------------------------------------------
struct TypeMetric : public Metric {
   TypeMetric() : Metric{"Type", 9} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getTypeName(); }
   std::partial_ordering compare(const Architecture& a, const Architecture& b) const override { return a.getTypeName() == b.getTypeName() ? std::partial_ordering::equivalent : a.getTypeName() < b.getTypeName() ? std::partial_ordering::less :
                                                                                                                                                                                                                    std::partial_ordering::greater; }
};
//--------------------------------------------------------------------------------
struct CPUVendorMetric : public Metric {
   CPUVendorMetric() : Metric{"CPUVendor", 9} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getPrimary().getCPUVendor(); }
};
//--------------------------------------------------------------------------------
struct StorageMetric : public Metric {
   StorageMetric() : Metric{"StorageDesc", 30} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getPageService().getDescription(); }
};
//--------------------------------------------------------------------------------
struct StorageDevice : public Metric {
   StorageDevice() : Metric{"StorageDev", 4} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getPageService().getDeviceType(); }
};
//--------------------------------------------------------------------------------
struct LogServiceMetric : public Metric {
   LogServiceMetric() : Metric{"LogDesc", 15} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getLogService().getDescription(); }
};
//--------------------------------------------------------------------------------
struct LogServicePrice : public Metric {
   LogServicePrice() : Metric{"LogSvcPrice"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override {
      auto price = Price::zero;
      if (!a.getPageService().containsLogService()) {
         price = a.getLogService().getPrice();
     }
     out << price;
   }
};
//--------------------------------------------------------------------------------
struct PageServicePrice : public Metric {
   PageServicePrice() : Metric{"PageSvcPrice"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getPageService().getPrice(); }
};
//--------------------------------------------------------------------------------
struct PrimaryPrice : public Metric {
   PrimaryPrice() : Metric{"PrimPrice"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getPrimary().getPrice(); }
};
//--------------------------------------------------------------------------------
struct EBSPrice : public Metric {
   EBSPrice() : Metric{"EBSPrice"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getPrimary().getEBSPrice(); }
};
//--------------------------------------------------------------------------------
struct SecondariesPrice : public Metric {
   SecondariesPrice() : Metric{"SecPrice"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getSecondaries().getPrice(); }
};
//--------------------------------------------------------------------------------
struct S3Price : public Metric {
   S3Price() : Metric{"S3Price"} {}
   static Price get(const Architecture& a) { return a.getS3Price(); }
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << get(a); }
};
//--------------------------------------------------------------------------------
struct NetworkPrice : public Metric {
   NetworkPrice() : Metric{"NetworkPrice"} {}
   static Price get(const Architecture& a) { return a.getNetworkPrice(); }
   void formatValue(std::ostream& out, const Architecture& a, bool) { out << get(a); }
};
//--------------------------------------------------------------------------------
struct TotalPrice : public Metric {
   TotalPrice() : Metric{"TotalPrice"} {}
   static Price getPrice(const Architecture& a) { return a.getTotalPrice(); }
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << getPrice(a); }
   std::partial_ordering compare(const Architecture& a, const Architecture& b) const override { return getPrice(a) <=> getPrice(b); }
};
//--------------------------------------------------------------------------------
struct DurabilityMetric : public Metric {
   Durability target;
   DurabilityMetric(Durability t) : Metric{"Durability", 8}, target{t} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getDurability(); }
   std::partial_ordering compare(const Architecture& a, const Architecture& b) const override { return a.getDurability() <=> b.getDurability(); }
   bool shouldExclude(const Architecture& a) const override { return a.getDurability() < target; }
};
//--------------------------------------------------------------------------------
struct DatasetSize : public Metric {
   uint64_t size;
   DatasetSize(uint64_t size) : Metric{"DataSize"}, size{size} {}
   void formatValue(std::ostream& out, const Architecture&, bool raw) { formatByte(out, size, raw); }
};
//--------------------------------------------------------------------------------
struct StorageCapacity : public Metric {
   StorageCapacity() : Metric{"Storage"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool raw) { formatByte(out, a.getPageService().getTotalSize(), raw); }
};
//--------------------------------------------------------------------------------
struct S3Storage : public Metric {
   S3Storage() : Metric{"S3Storage"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool raw) { formatByte(out, a.getS3Storage(), raw); }
};
//--------------------------------------------------------------------------------
struct PrimaryRandomLookupTx : public Metric {
   PrimaryRandomLookupTx() : Metric{"PrimLookups", 10} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getPrimaryRandomLookupTx(); }
};
//--------------------------------------------------------------------------------
struct SecondariesRandomLookupTx : public Metric {
   SecondariesRandomLookupTx() : Metric{"SecLookups"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getSecondariesRandomLookupTx(); }
};
//--------------------------------------------------------------------------------
struct RandomLookupTx : public Metric {
   Rate target;
   RandomLookupTx(Rate target) : Metric{"Lookups", 9}, target{target} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getRandomLookupTx(); }
   const char* getColor(const Architecture& a) const override {
      auto v = a.getRandomLookupTx();
      if (v == target) {
         return infra::Terminal::GREEN;
      } else {
         return infra::Terminal::NOCOLOR;
      }
   }
   bool shouldExclude(const Architecture& a) const override { return a.getRandomLookupTx() < target; }
};
//--------------------------------------------------------------------------------
struct RandomUpdateTx : public Metric {
   Rate target;
   RandomUpdateTx(Rate target) : Metric{"Updates"}, target{target} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getRandomUpdateTx(); }
   const char* getColor(const Architecture& a) const override {
      auto v = a.getRandomUpdateTx();
      if (v == target) {
         return infra::Terminal::GREEN;
      } else {
         return infra::Terminal::NOCOLOR;
      }
   }
   bool shouldExclude(const Architecture& a) const override { return a.getRandomUpdateTx() < target; }
};
//--------------------------------------------------------------------------------
struct PageWriteVolume : public Metric {
   PageWriteVolume() : Metric{"PageWriteVol"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool raw) override { formatByte(out, a.getPageService().getWriteVolume(), raw); }
};
//--------------------------------------------------------------------------------
struct PageReadVolume : public Metric {
   PageReadVolume() : Metric{"PageReadVol"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool raw) override { formatByte(out, a.getPageService().getReadVolume(), raw); }
};
//--------------------------------------------------------------------------------
// On the primary
struct NetworkInVolume : public Metric {
   NetworkInVolume() : Metric{"PrimNetIn"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool raw) override { formatByte(out, a.getPrimary().getNetworkInVolume(), raw); }
};
//--------------------------------------------------------------------------------
// On the primary
struct NetworkOutVolume : public Metric {
   NetworkOutVolume() : Metric{"PrimNetOut"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool raw) override { formatByte(out, a.getPrimary().getNetworkOutVolume(), raw); }
};
//--------------------------------------------------------------------------------
struct InterAZTraffic: public Metric {
   InterAZTraffic() : Metric{"InterAZ"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool raw) override { formatByte(out, a.getInterAZTraffic(), raw); }
};
//--------------------------------------------------------------------------------
struct LogVolume : public Metric {
   LogVolume() : Metric{"LogVolume"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool raw) override { formatByte(out, a.getPrimary().getLogVolume(), raw); }
};
//--------------------------------------------------------------------------------
struct S3Gets : public Metric {
   S3Gets() : Metric{"S3GET"} {}
  void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getS3GETRate(); }
};
//--------------------------------------------------------------------------------
struct S3Puts : public Metric {
   S3Puts() : Metric{"S3PUT"} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getS3PUTRate(); }
};
//--------------------------------------------------------------------------------
struct OpLatencyMetric : public Metric {
   Latency target;
   OpLatencyMetric(Latency latencyLimitNs) : Metric{"OpLatency", 7}, target{latencyLimitNs} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getOpLatency(); }
   bool shouldExclude(const Architecture& a) const override { return a.getOpLatency().avg > target.avg; }
   std::partial_ordering compare(const Architecture& a, const Architecture& b) const override { return a.getOpLatency().avg.count() <=> b.getOpLatency().avg.count(); }
};
//--------------------------------------------------------------------------------
struct CommitLatencyMetric : public Metric {
   CommitLatencyMetric() : Metric{"CommitLatency", 7} {}
   void formatValue(std::ostream& out, const Architecture& a, bool) override { out << a.getCommitLatency(); }
};
//--------------------------------------------------------------------------------
