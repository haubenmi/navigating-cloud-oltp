#include "HADRArchitecture.hpp"
#include "infra/Math.hpp"
#include <cassert>
#include <numeric>
#include <iomanip>
//--------------------------------------------------------------------------------
using namespace std;
using namespace infra;
//--------------------------------------------------------------------------------
HADR::HADR(const Parameter& p, Primary prim)
  : Architecture{p, prim, ArchType::HADR}, pageService{*InstanceStoragePageService::assemble(parameter, primary)}, logService{*InstanceStorageLogService::assemble(parameter, primary)}
{
   assert(secondaries.hasStandby());
   // Updates
   Rate cpuUpdates = primary.getCacheHitOps();
   auto pageWritesPerUpdate = primary.probEvictDirtyPageFromCache();
   auto logWritesPerUpdate = parameter.groupCommit ? ((parameter.getAriesLogRecordSize() * 1.0) / InstanceStorage::MaxIOPSize) : divRoundUp(parameter.getAriesLogRecordSize(), InstanceStorage::MaxIOPSize);
   auto writesPerUpdate = logWritesPerUpdate + pageWritesPerUpdate;
   auto readsPerUpdate = primary.probCacheMiss();
   auto readIops = primary.n.instanceStorage.getReadOps();
   auto writeIops = primary.n.instanceStorage.getWriteOps();

   auto readScale = readIops / readsPerUpdate;
   auto writeScale = writeIops / writesPerUpdate;

   auto networkPerUpdate = parameter.getAriesLogRecordSize() * secondaries.getCount();
   auto networkScale = primary.getNetworkOutLimit() / networkPerUpdate;
   updates = vmin(cpuUpdates, readScale, writeScale, networkScale, parameter.requiredUpdateOps);

   // Lookups; we can distribute over all secondaries, assume a secondary can do just as many lookups as the primary, as
   // they have to replay the WAL
   Rate cpuLookups = primary.getCacheHitOps(updates);
   auto writesPerLookup = primary.probEvictDirtyPageFromCache();
   auto readsPerLookup = primary.probCacheMiss();
   auto remainingWriteOps = writeIops - updates * writesPerUpdate;
   auto remainingReadOps = readIops - updates * readsPerUpdate;
   lookups = vmin(cpuLookups, remainingWriteOps / writesPerLookup, remainingReadOps / readsPerLookup, parameter.requiredLookupOps);

   secLookups = vmin(lookups * secondaries.availableForLookups(), parameter.requiredLookupOps - lookups);

   primary.networkOut = updates.rate * parameter.getAriesLogRecordSize() * secondaries.getCount();
   primary.logVolume = updates.rate * parameter.getAriesLogRecordSize();

   commitLatency = InstanceStorage::writeLatency;
   opLatency = Latency::combine({{primary.probCacheMiss(), InstanceStorage::readLatency}, {primary.probCacheHit(), primary.getCacheHitLatency()}});
}
//--------------------------------------------------------------------------------
uint64_t HADR::getInterAZTraffic() const {
   if (!parameter.deployAcrossAZ) return 0;
   // You want to distribute secondaries over AZs as much as possible to increase durability
   unsigned secondariesInSameAZ = secondaries.getCount() / parameter.numberOfAZs;
   auto x = secondaries.getCount() - secondariesInSameAZ;
   return x * updates.rate * parameter.getAriesLogRecordSize();
}
//--------------------------------------------------------------------------------
unique_ptr<HADR> HADR::assemble(const Parameter& p2, Node n) {
   auto p = p2;
   assert(p.indexOnlyTables);
   p.walIncludesUndo = true;
   // We require instance storage
   if (!n.instanceStorage) return {};
   Primary primary{p, n};

   auto size = p.getDataSize() + p.getRequiredAriesLogStorage();

   // We can distribute the lookups over all instances, so each instance only needs to be able to handle 1/N

   auto adjustedOps = p.requiredOpsPerNode();
   auto iopsPerPage = divRoundUp(p.pageSize, InstanceStorage::MaxIOPSize);
   auto pageWrites = adjustedOps * primary.probEvictDirtyPageFromCache() * iopsPerPage;
   auto pageReads = adjustedOps * primary.probCacheMiss() * iopsPerPage;
   auto logWrites = p.getLogWritesRequiredForUpdates(InstanceStorage::MaxIOPSize);
   auto networkWrites = p.requiredUpdateOps * p.getAriesLogRecordSize() * p.numSecondaries;
   auto& storage = primary.n.instanceStorage;

   if (networkWrites > primary.getNetworkOutLimit()) return {};
   if (size > storage.getUsableSize()) return {};
   if (pageReads > storage.getReadOps()) return {};
   if ((pageWrites + logWrites) > storage.getWriteOps()) return {};
   if (adjustedOps > primary.n.cpu.getOps(p.cpuCost)) return {};
   return make_unique<HADR>(p, primary);
}
//--------------------------------------------------------------------------------
Durability HADR::getDurability() const  {
  return Durability::calculateDurability(secondaries.getCount() + 1, primary.n.getAvailability().numericValue, parameter.getDataSize() / 50_mib, 1 /*We stay durable if one node survives*/);
}
//--------------------------------------------------------------------------------
FailoverTime HADR::getFailoverTime() const {
  auto throughput = vmin(primary.n.network.getReadLimit().rate, primary.n.instanceStorage.getReadThroughput(),primary.n.instanceStorage.getWriteThroughput());
  return Node::nodeSpinupTime +
    FailoverTime{parameter.getDataSize() / throughput};
}
//--------------------------------------------------------------------------------
