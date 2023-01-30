#include "ClassicArchitecture.hpp"
#include "infra/Math.hpp"
#include <cassert>
#include <numeric>
#include <iomanip>
//--------------------------------------------------------------------------------
using namespace std;
using namespace infra;
//--------------------------------------------------------------------------------
Classic::Classic(const Parameter& p, Primary prim)
   : Architecture{p, prim, ArchType::Classic}, pageService{*InstanceStoragePageService::assemble(parameter, primary)}, logService{*InstanceStorageLogService::assemble(parameter, primary)} {
   auto iopsPerPage = divRoundUp(parameter.pageSize, InstanceStorage::MaxIOPSize);

   // Updates
   Rate cpuUpdates = primary.n.cpu.getOps(parameter.cpuCost);
   auto pageWritesPerUpdate = primary.probEvictDirtyPageFromCache() * iopsPerPage;
   auto logWritesPerUpdate = parameter.groupCommit ? ((parameter.getAriesLogRecordSize() * 1.0) / InstanceStorage::MaxIOPSize) : divRoundUp(parameter.getAriesLogRecordSize(), InstanceStorage::MaxIOPSize);
   auto writesPerUpdate = pageWritesPerUpdate + logWritesPerUpdate;
   auto readsPerUpdate = primary.probCacheMiss() * iopsPerPage;
   auto readIops = primary.n.instanceStorage.getReadOps();
   auto writeIops = primary.n.instanceStorage.getWriteOps();

   auto readScale = readIops / readsPerUpdate;
   auto writeScale = writeIops / writesPerUpdate;
   updates = vmin(cpuUpdates, readScale, writeScale, parameter.requiredUpdateOps);

   // Lookups
   Rate cpuLookups = cpuUpdates - updates;
   auto writesPerLookup = primary.probEvictDirtyPageFromCache() * iopsPerPage;
   auto readsPerLookup = primary.probCacheMiss() * iopsPerPage;
   auto remainingWriteOps = writeIops - updates * writesPerUpdate;
   auto remainingReadOps = readIops - updates * readsPerUpdate;
   lookups = vmin(cpuLookups, remainingWriteOps / writesPerLookup, remainingReadOps / readsPerLookup, parameter.requiredLookupOps);

   primary.logVolume = updates.rate * parameter.getAriesLogRecordSize();
   commitLatency = InstanceStorage::writeLatency;
   // Assume all iops for the single page miss can be done in parallel, not increasing the latency
   opLatency = Latency::combine({{primary.probCacheMiss(), InstanceStorage::readLatency}, {primary.probCacheHit(), Memory::readLatency}});
}
//--------------------------------------------------------------------------------
unique_ptr<Classic> Classic::assemble(const Parameter& p2, const Node& n) {
   auto p = p2;
   assert(p.indexOnlyTables);
   p.walIncludesUndo = true;
   // We require instance storage
   if (!n.instanceStorage) return {};
   Primary primary{p, n};

   // Create an EBS device that fits both the database and the log
   auto size = p.getDataSize() + p.getRequiredAriesLogStorage();

   auto iopsPerPage = divRoundUp(p.pageSize, InstanceStorage::MaxIOPSize);
   auto pageWrites = p.requiredOps() * primary.probEvictDirtyPageFromCache() * iopsPerPage;
   auto pageReads = p.requiredOps() * primary.probCacheMiss() * iopsPerPage;
   auto logWrites = p.requiredUpdateOps * (p.groupCommit ? (p.getAriesLogRecordSize() * 1.0) / InstanceStorage::MaxIOPSize : divRoundUp(p.getAriesLogRecordSize(), InstanceStorage::MaxIOPSize));

   auto& storage = primary.n.instanceStorage;

   if (p.requiredOps() > primary.n.cpu.getOps(p.cpuCost)) return {};
   if (size > storage.getUsableSize()) return {};
   if (pageReads > storage.getReadOps()) return {};
   if ((pageWrites + logWrites) > storage.getWriteOps()) return {};

   return make_unique<Classic>(p, primary);
}
//--------------------------------------------------------------------------------
Durability Classic::getDurability() const {
  // The probability of being durable is that we are durable in each month
  double avail = primary.n.getAvailability().numericValue;
  double d = std::pow(avail, 12);
  return Durability{d};
}
//--------------------------------------------------------------------------------
FailoverTime Classic::getFailoverTime() const {
  auto& p = parameter;
  // start instance + download dataset + apply recovery
  // recovery depends on update rate
  // How often do we backup full database?
  FailoverTime download {p.getDataSize() / primary.n.network.getReadLimit().rate};
  FailoverTime diskWrite{(1.0 * p.getDataSize()) / primary.n.instanceStorage.getWriteThroughput()};
  FailoverTime recovery{(1.0 * p.getRequiredLogStorage()) / 100_mib};
  return Node::nodeSpinupTime + max(download,diskWrite) + recovery;
}
//--------------------------------------------------------------------------------
