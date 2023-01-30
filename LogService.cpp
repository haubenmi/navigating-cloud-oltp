#include "LogService.hpp"
#include "Architecture.hpp"
#include "infra/Math.hpp"
#include <sstream>
#include <iomanip>
//--------------------------------------------------------------------------------
using namespace std;
using namespace infra;
//--------------------------------------------------------------------------------
unique_ptr<InstanceStorageLogService> InstanceStorageLogService::assemble(const Parameter& p, Primary& prim) {
   auto inst = prim.reserveInstanceStorage(p.getRequiredLogStorage(), Rate::zero, p.getLogWritesRequiredForUpdates(InstanceStorage::MaxIOPSize));
   if (inst) {
      return make_unique<InstanceStorageLogService>(p, prim, *inst);
   } else {
      return nullptr;
   }
}
//--------------------------------------------------------------------------------
Rate InstanceStorageLogService::getUpdateOps() const {
   auto logWritesPerUpdate = parameter.groupCommit ? ((parameter.getLogRecordSize() * 1.0) / InstanceStorage::MaxIOPSize) : divRoundUp(parameter.getLogRecordSize(), InstanceStorage::MaxIOPSize);

   return storage.writes / logWritesPerUpdate;
}
//--------------------------------------------------------------------------------
InstanceStorageLogService::InstanceStorageLogService(const Parameter& p, Primary& prim, InstanceStorageAllotment inst)
   : LogService(p), primary{prim}, storage{inst} {}
//--------------------------------------------------------------------------------
Durability InstanceStorageLogService::getDurability() const {
   // The probability of being durable is that we are durable in each month
   double avail = primary.n.getAvailability().numericValue;
   double d = std::pow(avail, 12);
   return Durability{d};
}
//--------------------------------------------------------------------------------
Price EBSLogService::getPrice() const { return Price::zero; }
//--------------------------------------------------------------------------------
EBSLogService::EBSLogService(const Parameter& p, Primary& prim, EBSAllotment ebs)
   : LogService{p}, primary{prim}, ebs{ebs} {}
//--------------------------------------------------------------------------------
Rate EBSLogService::getUpdateOps() const {
   auto logWritesPerUpdate = parameter.groupCommit ? ((parameter.getLogRecordSize() * 1.0) / EBS::maxIopSize) : divRoundUp(parameter.getLogRecordSize(), EBS::maxIopSize);

   return ebs.iops / logWritesPerUpdate;
}
//--------------------------------------------------------------------------------
Durability EBSLogService::getDurability() const { return EBS::getDurability(ebs.type); }
//--------------------------------------------------------------------------------
unique_ptr<EBSLogService> EBSLogService::assemble(const Parameter& p, Primary& prim, EBS::Type t) {
   auto logWrites = p.getLogWritesRequiredForUpdates(EBS::maxIopSize);
   auto requiredBandwidth = p.requiredUpdateOps.rate * p.getLogRecordSize();
   auto logStorage = p.getRequiredLogStorage();

   if (auto ebs = prim.addEBSCapacity(t, logStorage, logWrites, requiredBandwidth, p.groupCommit ? EBS::maxIopSize : p.getLogRecordSize())) {
      return make_unique<EBSLogService>(p, prim, *ebs);
   }
   return nullptr;
}
//--------------------------------------------------------------------------------
string EBSLogService::getDescription() const { return ebs.describe(); }
//--------------------------------------------------------------------------------
string Ec2LogService::getDescription() const {
  stringstream res;
  res << setprecision(2) << logNodeFraction;
  res << "x" << logNode.name;
  return res.str();
}
//--------------------------------------------------------------------------------
double Ec2LogService::computeScale(const Parameter& p, const Node& logNode, unsigned logTargets) {
  if (p.requiredUpdateOps == Rate::zero) {
     return 0.0;
  }

  double storageScale = p.getRequiredLogStorage() * getReplication() / logNode.instanceStorage.getUsableSize();

  // The log node must be able to receive all writes via the network
  double networkReadScale = (p.requiredUpdateOps * getReplication()) / (logNode.network.getReadLimit() / p.getLogRecordSize());
  // We do not need to write log records instantly, but can rather batch them
  //  double logWriteScale = requiredLogWriteOps / logNode.instanceStorage.getWriteOps().rate;
  // The node must however be able to handle the throughput
  double logVolumeWriteScale = (p.requiredUpdateOps.rate * getReplication() * p.getLogRecordSize()) / logNode.instanceStorage.getWriteThroughput();
  double logNetworkWriteScale = (p.requiredUpdateOps * p.getLogRecordSize() * logTargets) / logNode.network.getWriteLimit();

  return vmaxafter(storageScale, networkReadScale, logVolumeWriteScale, logNetworkWriteScale);
}
//--------------------------------------------------------------------------------
// Uses EBS
uint64_t Ec2LogService::getMaxIopSize() const { return EBS::maxIopSize; }
//--------------------------------------------------------------------------------
unique_ptr<Ec2LogService> Ec2LogService::assemble(const Parameter& p, Primary& prim, const Node& logNode, unsigned replication) {

  auto logWrites = p.getLogWritesRequiredForUpdates(EBS::maxIopSize);
  auto throughput = p.requiredUpdateOps.rate * p.getLogRecordSize();
  auto size = p.getRequiredLogStorage();
  // Conceptually the EBS device belongs to the log service, but physically it is attached to the primary
  if (auto ebs = prim.addEBSCapacity(EBS::Type::io2, size, logWrites, throughput, p.groupCommit ? EBS::maxIopSize : p.getLogRecordSize())) {
     // Prohibit scaling over one log node for now
     auto logTargets = p.numSecondaries + replication;
     auto scale = computeScale(p, logNode, logTargets);
     if (scale > 1.0) return nullptr;
     return make_unique<Ec2LogService>(p, logNode, *ebs, scale, logTargets);
  }

  return nullptr;
}
//--------------------------------------------------------------------------------
Latency Ec2LogService::getCommitLatency() const { return EBS::writeLatency; }
//--------------------------------------------------------------------------------
Durability Ec2LogService::getDurability() const { return EBS::getDurability(logEBSDevice.type); }
//--------------------------------------------------------------------------------
Rate Ec2LogService::getUpdateOps() const {
   auto& p = parameter;

   // On the node, we stream the log records without committing them individually (thus we model group commit here)
   auto logServiceStorageWriteVolume = Rate::secondly((logNode.instanceStorage.getWriteThroughput() * logNodeFraction) / p.getLogRecordSize());
   auto logServiceNetworkReads = logNode.network.getReadLimit() / p.getLogRecordSize() * logNodeFraction;

   auto logServiceNetworkWrites = logNode.network.getWriteLimit() / p.getLogRecordSize() * logNodeFraction / targets;
   auto logDeviceThroughput = Rate::secondly(logEBSDevice.bandwidth / p.getLogRecordSize());
   auto result = vmin(logServiceStorageWriteVolume, logServiceNetworkReads, logServiceNetworkWrites, logDeviceThroughput);
   if (!p.groupCommit) {
      // On the log device, each commit is separate, thus we go for IOPS
      auto logDeviceWriteOps = logEBSDevice.iops;
      result = vmin(result, logDeviceWriteOps);
   }
   return result;
}
//--------------------------------------------------------------------------------
