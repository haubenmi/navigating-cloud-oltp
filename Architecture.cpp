#include "Architecture.hpp"
#include "infra/Config.hpp"
#include "infra/Math.hpp"
#include <numeric>
#include <iomanip>
//--------------------------------------------------------------------------------
using namespace std;
using namespace infra;
//--------------------------------------------------------------------------------
string archTypeToName(ArchType t) {
  switch(t) {
     case ArchType::Classic: return "classic";
     case ArchType::RemoteBlockDevice: return "rbd";
     case ArchType::AuroraLike: return "aurora";
     case ArchType::InMemory: return "inmem";
     case ArchType::HADR: return "hadr";
     case ArchType::SocratesLike: return "socrates";
     case ArchType::Dynamic: return "dynamic";
  }
  unreachable();
}
//--------------------------------------------------------------------------------
struct PairHasher {
  size_t operator()(auto v) const { return multihash(v.first, v.second); }
};
static std::unordered_map<pair<uint64_t, double>, double, PairHasher> dpTable;
//--------------------------------------------------------------------------------
double cacheHarmonic(uint64_t i, double alpha) {
  auto it = dpTable.find(pair(i,alpha));
  if (it != dpTable.end()) {
     return it->second;
  } else {
     double v = getGeneralizedHarmonicNumber(i, alpha);
     dpTable.insert({pair(i, alpha), v});
     return v;
  }
}
//--------------------------------------------------------------------------------
double getAccumulatedZipf(uint64_t k, uint64_t N, double alpha) {
  return cacheHarmonic(k, alpha) / cacheHarmonic(N, alpha);
}
//--------------------------------------------------------------------------------
string Primary::getDescription() const {
   string result =  n.name + (usesBufferPoolExtension ? "-rbpex" : "");
   // result += "{";
   // for (auto& e : ebs) {
   //   if (e) {
   //     result += e->getDescription();
   //   }
   // }
   // result += "}";
   return result;
}
//--------------------------------------------------------------------------------
Primary::Primary(const Parameter& p, const Node& n, bool rbpex) : p{p}, n{n}, usesBufferPoolExtension{rbpex} {
  if (p.lookupZipf != 0.0) {
     assert(p.indexOnlyTables); // Not implemented yet
     assert(p.requiredUpdateOps.rate == 0);
     auto cacheGB = dataInCache() / 1024 / 1024 / 1024;
     auto firstCacheGB = dataInFirstCache() / 1024 / 1024 / 1024;
     //     auto secondCacheGB = dataInSecondCache() / 1024 / 1024 / 1024;
     auto datasetGB = p.getDataSize() / 1024 / 1024 / 1024;
     probCacheHitVal = getAccumulatedZipf(cacheGB, datasetGB, p.lookupZipf);
     probFirstCacheHitVal = getAccumulatedZipf(firstCacheGB, datasetGB, p.lookupZipf);
     //     probSecondCacheHitVal = getAccumulatedZipf(secondCacheGB, datasetGB, p.lookupZipf);
     probSecondCacheHitVal = probCacheHitVal - probFirstCacheHitVal;
     //     assert(probFirstCacheHitVal + probSecondCacheHitVal == probCacheHitVal);
  } else {
     assert(dataInCache() <= p.getDataSize());
     probCacheHitVal = (1.0 * dataInCache()) / p.getDataSize();
     probIndexCacheHitVal = p.indexOnlyTables ? 1.0 : ((1.0 * indexInCache()) / p.indexSize());
     probFirstCacheHitVal = (1.0 * dataInFirstCache()) / p.getDataSize();
     probSecondCacheHitVal = (1.0 * dataInSecondCache()) / p.getDataSize();
  }
                                                                              }
//--------------------------------------------------------------------------------
optional<EBSAllotment> Primary::addEBSCapacity(EBS::Type t, uint64_t size, Rate iops2, uint64_t bandwidth, uint64_t iopSize) {
   auto iops = iops2.roundUp();
   assert(t != EBS::Type::io2x);
   // Compute potential devices
   Rate totalIops = Rate::zero;
   uint64_t totalThroughput = 0;
   uint64_t totalDevices = 0;
   for (unsigned i = 0; i < 4;++i) {
     auto& allot = ebsReserved[i];
     if (i == static_cast<unsigned>(t)) {
       auto tmp = EBS::createVolume(n.name, t, allot.size + size, (allot.iops + iops).nextInt(), allot.bandwidth + bandwidth, max(allot.maxIopSize,iopSize));
       totalIops += tmp.getIOPS();
       totalThroughput += tmp.getThroughput();
       totalDevices += tmp.getNumDevices();
     } else {
        auto& e = ebs[i];
        if (e) {
           totalIops += e->getIOPS();
           totalThroughput += e->getThroughput();
           totalDevices += e->getNumDevices();
        }
     }
   }

   if (totalIops > n.machineEbs.baseIops) return {};
   if (totalThroughput > n.machineEbs.baseThroughput) return {};
   if (totalDevices > n.maxEBSDevices()) return {};

   auto& allot = ebsReserved[static_cast<unsigned>(t)];
   allot.size += size;
   allot.iops += iops;
   allot.bandwidth += bandwidth;
   allot.maxIopSize = max(allot.maxIopSize, iopSize);
   getEBS(t).emplace(EBS::createVolume(n.name, t, allot.size, allot.iops.nextInt(), allot.bandwidth, allot.maxIopSize));
   return EBSAllotment{t, size, iops, bandwidth, iopSize};
}
//--------------------------------------------------------------------------------
Rate Primary::getCacheHitOps(Rate alreadyUsed) const {
      if (!usesBufferPoolExtension) return n.cpu.getOps(p.cpuCost) - alreadyUsed;
      auto iopsPerPage = divRoundUp(p.pageSize, InstanceStorage::MaxIOPSize);
      auto storagePageWrites = n.instanceStorage.getWriteOps() / iopsPerPage;
      auto storagePageReads = n.instanceStorage.getReadOps() / iopsPerPage;

      auto remainingWrites = storagePageWrites - alreadyUsed * probSecondCacheHit();
      auto remainingReads = storagePageReads - alreadyUsed * probSecondCacheHit();
      return vmin(n.cpu.getOps(p.cpuCost) - alreadyUsed, remainingWrites / probSecondCacheHit(), remainingReads / probSecondCacheHit());
   }
//--------------------------------------------------------------------------------
unique_ptr<Primary> Primary::assemble(const Parameter& p, const Node& n, bool rbpex) {
  auto res = make_unique<Primary>(p,n,rbpex);
  auto ops = p.requiredOpsPerNode();
  if (res->getCacheHitOps() < ops) return nullptr;
  return res;
}
//--------------------------------------------------------------------------------
Price Architecture::getTotalPriceImpl() const {
  auto price = getPrimary().getPrice();
  price += getPrimary().getEBSPrice();
  price += getSecondaries().getPrice();
  // Secondaries always have the same EBS devices as the primary
  price += getSecondaries().getCount() * getPrimary().getEBSPrice();
  price += getPageService().getPrice();
  if (!getPageService().containsLogService()) {
     price += getLogService().getPrice();
  }
  price += getNetworkPrice();
  price += getS3Price();

  cachedTotalPrice.emplace(price);
  return price;
}
//--------------------------------------------------------------------------------
static Price getS3StorageCost(const Architecture& a) {
  auto size = a.getS3Storage();

  auto cat1 = std::min(50_tib, size);
  size -= cat1;
  auto cat2 = std::min(450_tib, size);
  auto cat3 = size - cat2;

  return infra::divRoundUp(cat1, 1_gib) * S3::first50TBPerGB + infra::divRoundUp(cat2, 1_gib) * S3::next450TBPerGB + infra::divRoundUp(cat3, 1_gib) * S3::over500TBPerGB;
}
//--------------------------------------------------------------------------------
Price Architecture::getNetworkPrice() const {
  // The traffic is per second
  return (getInterAZTraffic() * 1.0 / 1_gib) * Network::interAZCost;
}
//--------------------------------------------------------------------------------
Price Architecture::getS3Price() const {
  auto price = getS3StorageCost(*this);
  price += S3::getPrice * getS3GETRate();
  price += S3::putPrice * getS3PUTRate();
  return price;
}
//--------------------------------------------------------------------------------
