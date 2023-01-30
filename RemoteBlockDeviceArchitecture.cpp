#include "RemoteBlockDeviceArchitecture.hpp"
#include "infra/Math.hpp"
//--------------------------------------------------------------------------------
using namespace std;
using namespace infra;
//--------------------------------------------------------------------------------
RemoteBlockDevice::RemoteBlockDevice(Parameter p, Primary prim, EBSAllotment ebs) : Architecture{p, prim, ArchType::RemoteBlockDevice}, pageService{parameter, primary, ebs, Rate::zero, Rate::zero}, log{parameter, primary, ebs} {
   // Updates
   Rate cpuUpdates = primary.n.cpu.getOps(parameter.cpuCost);
   auto pageWritesPerUpdate = primary.probEvictDirtyPageFromCache();
   auto logWritesPerUpdate = parameter.groupCommit ? ((parameter.getAriesLogRecordSize() * 1.0) / EBS::maxIopSize) : divRoundUp(parameter.getAriesLogRecordSize(), EBS::maxIopSize);
   auto writesPerUpdate = pageWritesPerUpdate + logWritesPerUpdate;
   auto readsPerUpdate = primary.probCacheMiss();
   auto totalIOPS = ebs.iops;
   double ebsScale = writesPerUpdate + readsPerUpdate; // > 1.0
   updates = vmin(cpuUpdates, totalIOPS / ebsScale, parameter.requiredUpdateOps);

   // Lookups
   Rate cpuLookups = cpuUpdates - updates;
   auto writesPerLookup = primary.probEvictDirtyPageFromCache();
   auto readsPerLookup = primary.probCacheMiss();
   auto remainingIops = totalIOPS - updates * ebsScale;
   lookups = vmin(cpuLookups, remainingIops / (readsPerLookup + writesPerLookup), parameter.requiredLookupOps);

   primary.logVolume = updates.rate * parameter.getAriesLogRecordSize();

   commitLatency = EBS::writeLatency;
   opLatency = Latency::combine({{primary.probCacheMiss(), EBS::readLatency}, {primary.probCacheHit(), Memory::readLatency}});
}
//--------------------------------------------------------------------------------
Durability RemoteBlockDevice::getDurability() const { return log.getDurability(); }
//--------------------------------------------------------------------------------
unique_ptr<RemoteBlockDevice> RemoteBlockDevice::assemble(const Parameter& p2, Node n, EBS::Type t) {
   auto p = p2;
   assert(p.indexOnlyTables);
   p.walIncludesUndo = true;
   Primary primary{p, n};

   // Create an EBS device that fits both the database and the log
   auto size = p.getDataSize() + p.getRequiredAriesLogStorage();

   auto pageWrites = p.requiredOps() * primary.probEvictDirtyPageFromCache();
   auto pageReads = p.requiredOps() * primary.probCacheMiss();
   auto logWrites = p.requiredUpdateOps * (p.groupCommit ? (p.getAriesLogRecordSize() * 1.0) / EBS::maxIopSize : divRoundUp(p.getAriesLogRecordSize(), EBS::maxIopSize));
   auto requiredIOPS = pageWrites + pageReads + logWrites;
   auto requiredBandwidth = (pageWrites + pageReads).nextInt() * p.pageSize + p.requiredUpdateOps.rate * p.getAriesLogRecordSize();

   auto ebs = primary.addEBSCapacity(t, size, requiredIOPS, requiredBandwidth, max(p.pageSize, p.tupleSize));
   if (!ebs) return {};
   assert(size <= ebs->size);
   if (p.requiredOps() > primary.n.cpu.getOps(p.cpuCost)) return {};
   return make_unique<RemoteBlockDevice>(p, primary, *ebs);
}
//--------------------------------------------------------------------------------
FailoverTime RemoteBlockDevice::getFailoverTime() const {
  // Cache warmup
  return Node::nodeSpinupTime + FailoverTime{1.0 * primary.dataInCache() / std::max(pageService.ebs.bandwidth,10ul)};
}
//--------------------------------------------------------------------------------
