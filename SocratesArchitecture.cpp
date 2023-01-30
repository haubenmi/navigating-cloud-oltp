#include "SocratesArchitecture.hpp"
#include "infra/Math.hpp"
#include <numeric>
#include <iomanip>
//--------------------------------------------------------------------------------
using namespace std;
using namespace infra;
//--------------------------------------------------------------------------------
Durability SocratesLike::durability = std::min(Durability{EBS::io2_durability}, S3::durability);
//--------------------------------------------------------------------------------
SocratesLike::SocratesLike(const Parameter& p, const Primary& prim, const Node& pageNode, unique_ptr<Ec2LogService> log)
  : Architecture{p, prim, ArchType::SocratesLike}, pageService{*Ec2PageService::assemble(parameter, primary, pageNode, Latency::deduce(parameter.requiredOpLatency, {{primary.probFirstCacheHit(), Memory::readLatency}, {primary.probSecondCacheHit(), InstanceStorage::readLatency}}), parameter.pageServerReplication)}, logService{*log} {
   // Higher chance for miss in the smaller cache

   // Updates
   // Case 1: page in buffer -> no miss
   // Case 2: page on SSD -> need a storage read, and evict a page
   // Case 3: page neither in buffer nor on SSD -> network read, and evict a page
   // Case 4: page in page server buffer
   // TODO: Model page server instanceStorage reads/writes
   Rate cpuUpdates = primary.n.cpu.getOps(parameter.cpuCost);
   auto networkLogWrites = primary.n.network.getWriteLimit() / parameter.getRedoLogRecordSize();
   auto networkPageReads = (primary.n.network.getReadLimit() / parameter.pageSize).roundDown();

   auto iopsPerPage = divRoundUp(parameter.pageSize, InstanceStorage::MaxIOPSize);
   auto storagePageWrites = primary.n.instanceStorage.getWriteOps() / iopsPerPage;
   auto storagePageReads = primary.n.instanceStorage.getReadOps() / iopsPerPage;

   // Log targets are all secondaries plus one page server
   auto logTargets = secondaries.getCount() + 1;

   updates = vmin(cpuUpdates,
                  networkLogWrites,
                  networkPageReads / primary.probCacheMiss(),
                  storagePageWrites / primary.probSecondCacheHit(), // We swap a page in case we find one in the disk cache portion
                  storagePageReads / primary.probSecondCacheHit(),
                  logService.getUpdateOps(),
                  parameter.requiredUpdateOps);
   // Lookups
   Rate cpuLookups = cpuUpdates - updates;
   auto networkPageReadsLookups = (networkPageReads - updates * primary.probCacheMiss()).roundDown();
   auto storagePageWritesLookups = storagePageWrites - updates * primary.probSecondCacheHit();
   auto storagePageReadsLookups = storagePageReads - updates * primary.probSecondCacheHit();

   lookups = vmin(cpuLookups,
                  networkPageReadsLookups / primary.probCacheMiss(),
                  storagePageWritesLookups / primary.probSecondCacheHit(),
                  storagePageReadsLookups / primary.probSecondCacheHit(),
                  parameter.requiredLookupOps);

   secLookups = vmin(lookups * secondaries.availableForLookups(), parameter.requiredLookupOps - lookups);

   primary.networkIn = (updates + lookups).rate * parameter.pageSize * primary.probCacheMiss();
   primary.networkOut = updates.rate * parameter.getRedoLogRecordSize(); // We only stream to one log service

   interAZTraffic = (updates + lookups + secLookups).rate * parameter.pageSize * primary.probCacheMiss();
   // The log service is in the same AZ as the primary
   // The log service has to distribute log records to all secondaries
   // Section 6: Socrates allows to deploy Secondaries and Page Servers in different data centers and availability zones.
   interAZTraffic += updates.rate * parameter.getRedoLogRecordSize() * logTargets;
   interAZTraffic *= parameter.getRemoteAZRatio();

   primary.logVolume = updates.rate * parameter.getRedoLogRecordSize();

   commitLatency = logService.getCommitLatency();

   opLatency = Latency::combine({{primary.probCacheHit(), primary.getCacheHitLatency()},
                                 {primary.probCacheMiss(),pageService.getOpLatency()}});
}
//--------------------------------------------------------------------------------
unique_ptr<SocratesLike> SocratesLike::assemble(const Parameter& p2, const Node& n, const Node& page, const Node& log, bool usesBufferPoolExtension) {
   auto p = p2;
   assert(p.indexOnlyTables);
   p.walIncludesUndo = false;
   auto adjustedOps = p.requiredOpsPerNode();

   if (!n.instanceStorage || n.instanceStorage.getUsableSize() < n.memory.getTotalSize()) return {}; // Socrates uses buffer pool extension
   // The p4d.24 has super fast networking, but not enough local IOPS, so RBPEX does not make sense
   if (n.name == "p4d.24") {
     usesBufferPoolExtension = false;
   }
   Primary primary{p, n, usesBufferPoolExtension};
   // Storage on page servers can be scaled infinitly, we don't need to check it here

   // For logging, we need to check if the EBS device of the log service can sustain the workload. The rest can scale up to one full instance
   auto logService = Ec2LogService::assemble(p, primary, log, p.pageServerReplication);
   if (!logService) return {};

   // We write log records to the log service
   auto networkWrites = p.requiredUpdateOps * p.getRedoLogRecordSize();
   auto networkReads = adjustedOps * primary.probCacheMiss();

   unsigned iopsPerPage = divRoundUp(p.pageSize, InstanceStorage::MaxIOPSize);
   auto storageWrites = adjustedOps * primary.probSecondCacheHit() * iopsPerPage;
   auto storageReads = adjustedOps * primary.probSecondCacheHit() * iopsPerPage;

   if (adjustedOps > primary.n.cpu.getOps(p.cpuCost)) return {};
   if (networkWrites > primary.n.network.getWriteLimit()) return {};
   if (networkReads > (primary.n.network.getReadLimit() / p.pageSize).roundDown()) return {};
   //   cerr << "secondaries: " << secondaries << "; storage writes: " << storageWrites << "; limit: " << primary.n.instanceStorage.getWriteOps() << "\n";
   if (storageWrites > primary.n.instanceStorage.getWriteOps()) return {};
   if (storageReads > primary.n.instanceStorage.getReadOps()) return {};

   return make_unique<SocratesLike>(p, primary, page, std::move(logService));
}
//--------------------------------------------------------------------------------
FailoverTime SocratesLike::getFailoverTime() const {
  //  if (secondaries.hasStandby()) return Node::secondaryTakeover;
  // Even if there are no secondaries, database just needs to warm its cache, assume we can do that with network bandwidth
  uint64_t byteInViaNetwork = min(primary.n.network.getReadLimit(), pageService.getPageReadOps() * parameter.pageSize).rate;

  return Node::nodeSpinupTime + FailoverTime{1.0 * primary.dataInFirstCache() / byteInViaNetwork} + FailoverTime{1.0 * primary.dataInSecondCache() / min(byteInViaNetwork, primary.n.instanceStorage.getWriteThroughput())};
}
//--------------------------------------------------------------------------------
