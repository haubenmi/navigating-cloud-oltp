
#pragma once
#include "Common.hpp"
#include "Resources.hpp"
#include "PageService.hpp"
#include "LogService.hpp"
#include <array>
#include <iosfwd>
#include <memory>
#include <optional>
#include <set>
#include <unordered_set>
#include <variant>
#include <cassert>
//--------------------------------------------------------------------------------//
enum class ArchType : uint8_t {
   Classic,
   HADR, // Log shipping directly to secondaries
   RemoteBlockDevice,
   InMemory,
   AuroraLike, // No dirty page writing, only redo log to page servers
   SocratesLike, // Dirty page writing only for availability, log to service
   Dynamic
};
//--------------------------------------------------------------------------------
std::string archTypeToName(ArchType);
//--------------------------------------------------------------------------------
struct Primary {
   Parameter p;
   Node n;
   std::array<std::optional<EBS>, 4> ebs;
   std::array<EBSAllotment, 4> ebsReserved;
   std::optional<EBS>& getEBS(EBS::Type t) { return ebs[static_cast<unsigned>(t)]; }
   bool usesBufferPoolExtension = false;
   InstanceStorageAllotment reserved;
   double probFirstCacheHitVal;
   double probSecondCacheHitVal;
   double probCacheHitVal;
   double probIndexCacheHitVal;
   // uint64_t instanceStorageReserved = 0;
   // Rate instanceStorageReservedReadOps = Rate::zero;
   // Rate instanceStorageReservedWriteOps = Rate::zero;

   uint64_t networkIn = 0;
   uint64_t networkOut = 0;
   uint64_t logVolume = 0;

   Primary(const Parameter& p, const Node& n, bool rbpex = false);
   static std::unique_ptr<Primary> assemble(const Parameter& p, const Node& n, bool rbpex = false);
   Primary(const Primary& p) = default;
   std::string getDescription() const;
   Price getEBSPrice() const {
      Price result = Price::zero;
      for (auto& e : ebs) {
         if (e) {
            result += e->getPrice();
         }
      }
      return result;
   }
   Price getPrice() const { return n.price; }

   std::optional<EBSAllotment> addEBSCapacity(EBS::Type t, uint64_t size, Rate iops, uint64_t bandwidth, uint64_t iopSize);

   std::optional<InstanceStorageAllotment> reserveInstanceStorage(uint64_t size, Rate reads, Rate writes) {
      if (usesBufferPoolExtension) return std::nullopt;
      //      if (!n.instanceStorage && (size != 0 || reads != Rate::zero || writes != Rate::zero)) return std::nullopt;
      if (reserved.size + size > n.instanceStorage.getUsableSize()) return std::nullopt;
      if (reserved.reads + reads > n.instanceStorage.getReadOps()) return std::nullopt;
      if (reserved.writes + writes > n.instanceStorage.getWriteOps()) return std::nullopt;
      reserved.size += size;
      reserved.reads += reads;
      reserved.writes += writes;
      return InstanceStorageAllotment{size, reads, writes};
   }
   std::string getCPUVendor() const { return n.cpu.vendor; }
   Rate getNetworkInLimit() const { return n.network.getReadLimit(); }
   Rate getNetworkOutLimit() const { return n.network.getWriteLimit(); }

   uint64_t getNetworkInVolume() const { return networkIn; }
   uint64_t getNetworkOutVolume() const { return networkOut; }
   uint64_t getLogVolume() const { return logVolume; }

   Rate getCacheHitOps(Rate alreadyUsed = Rate::zero) const;
   Latency getCacheHitLatency() const {
      if (!usesBufferPoolExtension) return Memory::readLatency;
      return Latency::combine({{probFirstCacheHit()/probCacheHit(), Memory::readLatency}, {probSecondCacheHit()/probCacheHit(), InstanceStorage::readLatency}});
   }
   uint64_t getBufferCacheSize() const { return (n.memory.getTotalSize() * p.usableMemory) + (usesBufferPoolExtension ? n.instanceStorage.getUsableSize() : 0); }
   double probDirty() const { return p.requiredUpdateOps / (p.requiredUpdateOps + p.requiredLookupOps); }
  // 10GB data, 20GB index, 100GB RAM
   uint64_t dataInCache() const {
      assert(indexInCache() <= getBufferCacheSize());
      return std::min(getBufferCacheSize() - indexInCache(), p.getDataSize());
   }
   uint64_t indexInCache() const { return std::min(getBufferCacheSize(), p.indexSize()); }
   uint64_t dataNotInCache() const { return p.getDataSize() - dataInCache(); }

   double probFirstCacheHit() const { return probFirstCacheHitVal; }
   double probSecondCacheHit() const { return probSecondCacheHitVal; }
   double probCacheHit() const { return probCacheHitVal; }
   double probCacheMiss() const { return 1.0 - probCacheHit(); }
   double probIndexCacheHit() const { return probIndexCacheHitVal; }
   double probIndexCacheMiss() const { return std::max(0.0, 1.0 - probIndexCacheHit()); }

   double probEvictDirtyPageFromCache() const { return probCacheMiss() * probDirty(); }

   uint64_t dataInFirstCache() const { return std::min(static_cast<uint64_t>(n.memory.getTotalSize() * p.usableMemory), p.getDataSize()); }
   uint64_t dataNotInFirstCache() const { return p.getDataSize() - dataInFirstCache(); }

   uint64_t dataInSecondCache() const { return usesBufferPoolExtension ? std::min(n.instanceStorage.getUsableSize(), dataNotInFirstCache()) : 0; }
   uint64_t dataNotInSecondCache() const { return p.getDataSize() - dataInSecondCache(); }

   // double firstCacheMissSecondCacheHit() const {
   //    // With buffer cache extension, both caches are combined into an uniform one
   //    return probCacheHit() * (1.0 * dataInSecondCache() / dataInCache());
   // }
};
//--------------------------------------------------------------------------------
class Secondaries {
  const unsigned count;
  Node n;

public:
  Secondaries(unsigned c, Node n) : count{c}, n{n} {}
  //  operator bool() const { return count != 0; }
  Price getPrice() const { return count * n.getPrice(); } // Make secondaries a bit more expensive for correct sorting
  bool hasStandby() const { return count > 0; }
  unsigned availableForLookups() const { return (count > 0) ? (count - 1) : 0; }
  unsigned getCount() const { return count; }
};
//--------------------------------------------------------------------------------
// Restrict to certain instance types?
// https://leaderboard.vantage.sh/
struct Architecture {
   protected:
   ArchType type;
   Parameter parameter;
   Primary primary;
   Secondaries secondaries;
   Latency opLatency;
   Latency commitLatency;
   mutable std::optional<Price> cachedTotalPrice;

   public:
   Architecture(const Parameter& p, const Primary& prim, ArchType t) : type{t}, parameter{p}, primary{prim}, secondaries{parameter.numSecondaries, prim.n} {}
   virtual ~Architecture() = default;
   std::string getTypeName() const { return archTypeToName(type); }
   ArchType getType() const { return type; }

   Price getTotalPrice() const {
      if (cachedTotalPrice) {
         return *cachedTotalPrice;
      } else {
         return getTotalPriceImpl();
      }
   }
   Price getTotalPriceImpl() const;
   Price getS3Price() const;
   Price getNetworkPrice() const;

   virtual const Primary& getPrimary() const { return primary; }
   virtual const Secondaries& getSecondaries() const { return secondaries; }
   virtual const PageService& getPageService() const {
      static NoopPageService n(parameter);
      return n;
   }
   virtual const LogService& getLogService() const {
      static NoopLogService l{parameter};
      return l;
   }
   virtual uint64_t getS3Storage() const = 0;
   virtual uint64_t getInterAZTraffic() const = 0;
   virtual Rate getS3GETRate() const = 0;
   virtual Rate getS3PUTRate() const = 0;
   virtual Durability getDurability() const = 0;
   virtual FailoverTime getFailoverTime() const { return FailoverTime{999990}; }
   virtual Rate getPrimaryRandomLookupTx() const = 0;
   virtual Rate getSecondariesRandomLookupTx() const { return Rate::zero; }
   Rate getRandomLookupTx() const { return getPrimaryRandomLookupTx() + getSecondariesRandomLookupTx(); }
   virtual Rate getRandomUpdateTx() const = 0;

   virtual Latency getOpLatency() const { return opLatency; }
   virtual Latency getCommitLatency() const { return commitLatency; }
};
//--------------------------------------------------------------------------------
