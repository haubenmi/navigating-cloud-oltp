#include "AuroraArchitecture.hpp"
#include "infra/Math.hpp"
#include <numeric>
#include <iomanip>
//--------------------------------------------------------------------------------
using namespace std;
using namespace infra;
//--------------------------------------------------------------------------------
AuroraLike::AuroraLike(Parameter p, Node n, Node storageNode) : Architecture{p, Primary{p, n}, ArchType::AuroraLike}, storageService{*CombinedPageServiceLog::assemble(parameter, primary, storageNode, Latency::deduce(parameter.requiredOpLatency, {{primary.probCacheHit(), primary.getCacheHitLatency()}}))} {
   // Updates
   Rate cpuUpdates = primary.n.cpu.getOps();

   // Limits on the write path
   auto primaryNetworkWrites = primary.n.network.getWriteLimit() / p.getRedoLogRecordSize();
   auto storageWrites = storageService.getUpdateOps();
   // We need to stream the log record 6 times to storage and to all replicas
   auto updateLimitViaWrites = std::min(primaryNetworkWrites / (CombinedPageServiceLog::replication + secondaries.getCount()), storageWrites);

   // Limits on the read path
   auto primaryNetworkReads = primary.n.network.getReadLimit() / p.pageSize;
   auto storageReads = storageService.getPageReadOps();
   auto possibleReads = std::min(primaryNetworkReads, storageReads);
   auto updateLimitViaReads = possibleReads / primary.probCacheMiss();

   updates = vmin(cpuUpdates, updateLimitViaWrites, updateLimitViaReads, p.requiredUpdateOps);

   // Lookups
   auto cpu = cpuUpdates - updates;
   auto remainingStorageReads = storageReads - updates * primary.probCacheMiss();
   auto remainingNetworkReads = (primary.n.network.getReadLimit() - updates * p.pageSize * primary.probCacheMiss()) / p.pageSize;
   lookups = vmin(cpu, remainingStorageReads / primary.probCacheMiss(), remainingNetworkReads / primary.probCacheMiss(), p.requiredLookupOps);

   secLookups = vmin(lookups * secondaries.availableForLookups(), p.requiredLookupOps - lookups);

   primary.networkIn = (updates + lookups).rate * p.pageSize * primary.probCacheMiss();
   primary.networkOut = updates.rate * p.getRedoLogRecordSize() * (secondaries.getCount() + CombinedPageServiceLog::replication);

   // Both page reads and log writes
   interAZTraffic = (updates + lookups + secLookups).rate * p.pageSize * primary.probCacheMiss();
   interAZTraffic += primary.networkOut;
   interAZTraffic *= p.getRemoteAZRatio();

   primary.logVolume = updates.rate * p.getRedoLogRecordSize();

   commitLatency = storageService.getCommitLatency();
   opLatency = Latency::combine({{primary.probCacheHit(), Memory::readLatency},
                                 {primary.probCacheMiss(),storageService.getOpLatency()}});
}
//--------------------------------------------------------------------------------
unique_ptr<AuroraLike> AuroraLike::assemble(const Parameter& p2, const Node& n, const Node& s) {
   auto p = p2;
   p.walIncludesUndo = false;
   // We require instance storage on the storage node
   assert(s.instanceStorage);

   Primary primary{p, n};
   // No limits on the storage service, it can scale arbitrarily
   auto adjustedOps = p.requiredOpsPerNode();

   auto networkWrites = p.requiredUpdateOps * (CombinedPageServiceLog::replication + p.numSecondaries) * p.getRedoLogRecordSize(); // Log entry for each update
   auto networkReads = adjustedOps * primary.probCacheMiss() * p.pageSize; // Page load for each cache miss

   if (adjustedOps > primary.n.cpu.getOps()) return {};
   if (networkWrites > primary.n.network.getWriteLimit()) return {};
   if (networkReads > primary.n.network.getReadLimit()) return {};

   return make_unique<AuroraLike>(p, n, s);
}
//--------------------------------------------------------------------------------
Durability AuroraLike::getDurability() const { return storageService.getDurability(); }
//--------------------------------------------------------------------------------
FailoverTime AuroraLike::getFailoverTime() const {
  //  if (secondaries.hasStandby()) return Node::secondaryTakeover;
  // Even if there are no secondaries, database just needs to warm its cache, assume we can do that with network bandwidth
  Rate byteInViaNetwork = min(primary.n.network.getReadLimit(), storageService.getPageReadOps() * parameter.pageSize);
  return Node::nodeSpinupTime +
    FailoverTime{primary.dataInCache() / byteInViaNetwork.rate};
}
//--------------------------------------------------------------------------------
