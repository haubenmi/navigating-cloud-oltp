#include "PageService.hpp"
#include "Architecture.hpp"
#include "infra/Math.hpp"
#include "AuroraArchitecture.hpp"
#include <numeric>
#include <iomanip>
//--------------------------------------------------------------------------------
using namespace std;
using namespace infra;
//--------------------------------------------------------------------------------
PageService::PageService(const Parameter& p) : parameter{p} {}
//--------------------------------------------------------------------------------
PageService::PageService(const PageService& p) : parameter{p.parameter} {}
//--------------------------------------------------------------------------------
string InstanceStoragePageService::getDescription() const { return primary.n.instanceStorage.getDescription(); }
//--------------------------------------------------------------------------------
string InstanceStoragePageService::getDeviceType() const { return primary.n.instanceStorage.storageTypeToString(); }
//--------------------------------------------------------------------------------
uint64_t InstanceStoragePageService::getTotalSize() const { return storage.size; }
//--------------------------------------------------------------------------------
Latency InstanceStoragePageService::getOpLatency() const { return InstanceStorage::readLatency; }
//--------------------------------------------------------------------------------
unique_ptr<InstanceStoragePageService> InstanceStoragePageService::assemble(const Parameter& p, Primary& primary) {
   auto size = p.getDataSize();
   auto iopsPerPage = divRoundUp(p.pageSize, InstanceStorage::MaxIOPSize);
   auto pageWrites = p.requiredOpsPerNode() * primary.probEvictDirtyPageFromCache() * iopsPerPage;
   auto pageReads = p.requiredOpsPerNode() * primary.probCacheMiss() * iopsPerPage;
   auto inst = primary.reserveInstanceStorage(size, pageReads.roundUp(), pageWrites.roundUp());
   if (inst) {
      return make_unique<InstanceStoragePageService>(p, primary, *inst);
   } else {
      return nullptr;
   }
}
//--------------------------------------------------------------------------------
InstanceStoragePageService::InstanceStoragePageService(const Parameter& p2, Primary& prim, InstanceStorageAllotment inst)
   : PageService{p2}, primary{prim}, storage{inst} {
}
//--------------------------------------------------------------------------------
unique_ptr<InMemoryPageService> InMemoryPageService::assemble(const Parameter& p, Primary& prim) {
   if (prim.n.memory.getTotalSize() < p.getDataSize()) return nullptr;
   return make_unique<InMemoryPageService>(p, prim);
}
//--------------------------------------------------------------------------------
unique_ptr<Ec2PageService> Ec2PageService::assemble(const Parameter& p, Primary& prim, Node pageNode, Latency targetLatency, [[maybe_unused]] unsigned replication, bool useRbpex) {
   assert(pageNode.instanceStorage.devices > 0.0);

   double storageScale = (1.0 * replication * p.getDataSize()) / (pageNode.instanceStorage.getUsableSize() + (useRbpex ? pageNode.memory.getTotalSize() : 0));

   // A page server reads a log record from the network for every log record that gets applied
   double networkReadScale = (p.requiredUpdateOps * replication * p.getLogRecordSize()) / pageNode.network.getReadLimit();

   // Latency is unaffected by the rbpex, as a good implementation will move the disk writes from the hot path, and we still only
   // have the storage read latency on the cache miss path
   Latency network = Latency::combine({{p.getSameAZRatio(), SameDatacenter::latency}, {p.getRemoteAZRatio(), SameRegion::latency}});
   double minRequiredCacheHitRate = Latency::getRatio(targetLatency - network.asAvg(), Memory::readLatency, InstanceStorage::readLatency);
   auto memoryScaleForLatency = (replication * p.getDataSize() * minRequiredCacheHitRate) / pageNode.memory.getTotalSize();

   auto iopsPerPage = divRoundUp(p.pageSize, InstanceStorage::MaxIOPSize);
   Rate requiredPageNodeGets = p.requiredOps() * prim.probCacheMiss();
   double networkWriteScale = requiredPageNodeGets / (pageNode.network.getWriteLimit() / p.pageSize);
   // TODO: Model storage writes
   // Other TODO: The Socrates page service uses rbpex

   if (useRbpex) {
      // Page either in memory or
      // on disk. if on disk, we need both a page read and write
   }

   // Solving the equation: T < (M*f/D) * MemOps + (D-M*f)/D * DiskOps towards f   where D=data,M=Mem,f=scale
   // gives us: f > ((T-DiskOps) * Data) / (MemOps - DiskOps) * Mem

   // With rbpex, for each cache miss we have to write a page to disk
   auto writeOps = pageNode.instanceStorage.getWriteOps() / iopsPerPage;
   auto readOps = pageNode.instanceStorage.getReadOps() / iopsPerPage;
   auto diskOps = useRbpex ? min(writeOps, readOps) : readOps;
   double iopsScale = (requiredPageNodeGets * p.getDataSize()) / (diskOps * p.getDataSize() + requiredPageNodeGets * pageNode.memory.getTotalSize());

   auto pageNodeFraction = vmaxafter(storageScale, networkReadScale, networkWriteScale, iopsScale, memoryScaleForLatency);
   // Quick hack to get around rounding issues:
   pageNodeFraction *= 1.0001;
   return make_unique<Ec2PageService>(p, pageNode, pageNodeFraction, useRbpex);
}
//--------------------------------------------------------------------------------
string Ec2PageService::getDescription() const {
  stringstream res;
  res << setprecision(2) << pageNodeFraction;
  res << "x" << pageNode.name;
  if (useRbpex) res << "-rbpex";

  return res.str();
}
//--------------------------------------------------------------------------------
double Ec2PageService::getPageNodeCacheMiss() const {
  auto dataInPageNodeCache = min(parameter.getDataSize(), static_cast<uint64_t>(pageNodeFraction * pageNode.memory.getTotalSize()));
  double pageNodeCacheMiss = (1.0 * (parameter.getDataSize() - dataInPageNodeCache)) / parameter.getDataSize();
  assert(0 <= pageNodeCacheMiss && pageNodeCacheMiss <= 1.0);
  return pageNodeCacheMiss;
}
//--------------------------------------------------------------------------------
Latency Ec2PageService::getOpLatency() const {
  auto pageNodeCacheMiss = getPageNodeCacheMiss();
  Latency network = Latency::combine({{parameter.getSameAZRatio(),SameDatacenter::latency}, {parameter.getRemoteAZRatio(), SameRegion::latency}});
  Latency pageAccess = Latency::combine({{pageNodeCacheMiss, InstanceStorage::readLatency}, {1.0 - pageNodeCacheMiss, Memory::readLatency}});
  return network.asAvg() + pageAccess;
}
//--------------------------------------------------------------------------------
Rate Ec2PageService::getPageReadOps() const {
   auto iopsPerPage = divRoundUp(parameter.pageSize, InstanceStorage::MaxIOPSize);
   auto diskReads = pageNode.instanceStorage.getReadOps() * pageNodeFraction / iopsPerPage;
   auto diskWrites = pageNode.instanceStorage.getWriteOps() * pageNodeFraction / iopsPerPage;
   auto diskOps = useRbpex ? min(diskReads, diskWrites) : diskReads;
   Rate pageNodeStorageLimit = diskOps / getPageNodeCacheMiss();

   Rate pageNodeNetworkOutLimit = (pageNode.network.getWriteLimit() * pageNodeFraction) / parameter.pageSize;

   return vmin(pageNodeStorageLimit, pageNodeNetworkOutLimit);
}
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
string EBSPageService::getDescription() const { return ebs.describe(); }
//--------------------------------------------------------------------------------
string EBSPageService::getDeviceType() const { return EBS::getTypeName(ebs.type); }
//--------------------------------------------------------------------------------
// Price attributed to primary
Price EBSPageService::getPrice() const { return Price::zero; }
//--------------------------------------------------------------------------------
uint64_t EBSPageService::getTotalSize() const { return ebs.size; }
//--------------------------------------------------------------------------------
EBSPageService::EBSPageService(const Parameter& p, Primary& prim, EBSAllotment ebs, Rate reads, Rate writes)
   : PageService(p), primary(prim), ebs(ebs), reads{reads}, writes{writes} {}
//--------------------------------------------------------------------------------
unique_ptr<EBSPageService> EBSPageService::assemble(const Parameter& parameter, Primary& prim, EBS::Type t) {
   auto size = parameter.getDataSize();
   auto iopsPerPage = divRoundUp(parameter.pageSize, EBS::maxIopSize);
   // Hack to get around rounding issues
   auto pageWrites = parameter.requiredOps() * prim.probEvictDirtyPageFromCache() * iopsPerPage * 1.001;
   auto pageReads = parameter.requiredOps() * prim.probCacheMiss() * iopsPerPage * 1.001;
   auto iops = pageWrites + pageReads;

   auto bandwidth = iops.nextInt() * parameter.pageSize;
   if (auto ebs = prim.addEBSCapacity(t, size, iops, bandwidth, parameter.pageSize)) {
      return make_unique<EBSPageService>(parameter, prim, *ebs, pageReads, pageWrites);
   } else {
      return nullptr;
   }
}
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
Durability CombinedPageServiceLog::getDurability() const {
  // The aurora paper claims they can repair 10GB in 10 sec on a 10Gbit NIC.
  // We just take those 10sec for now
  return Durability::calculateDurability(replication, n.getAvailability().numericValue, 10, 3 /*we always need to maintain read quorum to be durable*/);
}
//--------------------------------------------------------------------------------
unique_ptr<CombinedPageServiceLog> CombinedPageServiceLog::assemble(const Parameter& p, Primary& prim, Node storageNode, Latency targetLatency) {
   double grossStorageSize = (p.getDataSize() + p.indexSize()) * AuroraLike::dataReplication + p.getRequiredLogStorage() * AuroraLike::logReplication;
   assert(storageNode.instanceStorage);
   // No divRoundUp here, we model a multi-tenant service!
   double datasetScale = grossStorageSize / storageNode.instanceStorage.getUsableSize();

   Latency network = Latency::combine({{p.getSameAZRatio(), SameDatacenter::latency}, {p.getRemoteAZRatio(), SameRegion::latency}});
   double minRequiredCacheHitRate = Latency::getRatio(targetLatency - network.asAvg(), Memory::readLatency, InstanceStorage::readLatency);
   auto latencyScale = (p.getDataSize() * minRequiredCacheHitRate) / storageNode.memory.getTotalSize();

   //  assert(!p.walIncludesUndo); // Undo makes no sense here
   Rate requiredStorageWriteOps = p.requiredUpdateOps * replication;
   double networkReadScale = (requiredStorageWriteOps * p.getRedoLogRecordSize()) / storageNode.network.getReadLimit();

   Rate adjustedStorageWriteOps = p.getLogWritesRequiredForUpdates(InstanceStorage::MaxIOPSize) * replication;
   double storageWriteScale = adjustedStorageWriteOps / storageNode.instanceStorage.getWriteOps();

   auto iopsPerPage = divRoundUp(p.pageSize, InstanceStorage::MaxIOPSize);
   // Do not take opsPerNode here, we need to handle requests from the primary and all secondaries here in the storage layer
   Rate requiredPageNodeGets = p.requiredOps() * (prim.probCacheMiss() + prim.probIndexCacheMiss());
   auto diskOps = storageNode.instanceStorage.getReadOps() / iopsPerPage;
   auto memSize = storageNode.memory.getTotalSize();
   double iopsScale = (requiredPageNodeGets * p.getDataSize()) / (diskOps * p.getDataSize() + requiredPageNodeGets * memSize);

   double networkWriteScale = requiredPageNodeGets / (storageNode.network.getWriteLimit() / p.pageSize);

   auto fraction = vmaxafter(datasetScale, networkReadScale, storageWriteScale, networkWriteScale, iopsScale, latencyScale);
   // Accomodate for floating point errors
   fraction *= 1.0001;

   return make_unique<CombinedPageServiceLog>(p, storageNode, fraction);
}
//--------------------------------------------------------------------------------
string CombinedPageServiceLog::getDescription() const {
  stringstream res;
  res << "comb-p+l(";
  res << setprecision(2) << fraction;
  res << "x" << n.name;
  res << ")";
  return res.str();
}
//--------------------------------------------------------------------------------
double CombinedPageServiceLog::getPageNodeCacheMiss() const {
   auto& p = PageService::parameter;
   auto dataInPageNodeCache = min(p.getDataSize(), static_cast<uint64_t>(fraction * n.memory.getTotalSize()));
   double pageNodeCacheMiss = (1.0 * (p.getDataSize() - dataInPageNodeCache)) / p.getDataSize();
   assert(0 <= pageNodeCacheMiss && pageNodeCacheMiss <= 1.0);
   return pageNodeCacheMiss;
}
//--------------------------------------------------------------------------------
Rate CombinedPageServiceLog::getPageReadOps() const {
   auto iopsPerPage = divRoundUp(PageService::parameter.pageSize, InstanceStorage::MaxIOPSize);
   auto storageReads = (n.instanceStorage.getReadOps() * fraction) / iopsPerPage / getPageNodeCacheMiss();
   auto networkWrites = n.network.getWriteLimit() * fraction / PageService::parameter.pageSize;
   return vmin(storageReads, networkWrites);
}
//--------------------------------------------------------------------------------
Rate CombinedPageServiceLog::getUpdateOps() const {
   auto& p = PageService::parameter;
   auto possibleStorageWrites = n.instanceStorage.getWriteOps() * fraction;
   auto storageWritesPerUpdate = p.groupCommit ? ((p.getLogRecordSize() * 1.0) / InstanceStorage::MaxIOPSize)
     : divRoundUp(p.getLogRecordSize(), InstanceStorage::MaxIOPSize);
   auto storageWrites = possibleStorageWrites / storageWritesPerUpdate;
   auto networkReads = n.network.getReadLimit() * fraction / p.getLogRecordSize();
   return vmin(storageWrites, networkReads) / replication;
}
//--------------------------------------------------------------------------------
Latency CombinedPageServiceLog::getCommitLatency() const {
   auto& p = PageService::parameter;
   // Write quorum requires the write to reach 4/6 replicas
   // We just use the max for now to simulate the quorum
   Latency network = p.deployAcrossAZ ? Latency{SameRegion::latency.max} : Latency{SameDatacenter::latency.max};
   //  Latency network = Latency::combine({{parameter.getSameAZRatio(), SameDatacenter::latency}, {parameter.getRemoteAZRatio(), SameRegion::latency}});
   Latency pageWrite = InstanceStorage::writeLatency;
   return network + pageWrite;
}
//--------------------------------------------------------------------------------
Latency CombinedPageServiceLog::getOpLatency() const {
   auto& p = PageService::parameter;
   // We don't need quorum, just ask a single instance
   // We still have a chance of hitting a remote instance
   Latency network = Latency::combine({{p.getSameAZRatio(), SameDatacenter::latency}, {p.getRemoteAZRatio(), SameRegion::latency}});
   auto cacheMiss = getPageNodeCacheMiss();
   Latency pageAccess = Latency::combine({{cacheMiss, InstanceStorage::readLatency}, {1.0 - cacheMiss, Memory::readLatency}});
   return network + pageAccess;
}
//--------------------------------------------------------------------------------
