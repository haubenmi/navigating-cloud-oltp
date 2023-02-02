#include "InMemArchitecture.hpp"
#include "infra/Math.hpp"
//--------------------------------------------------------------------------------
using namespace std;
using namespace infra;
//--------------------------------------------------------------------------------
static Parameter adjustParams(Parameter p) {
  p.walIncludesUndo = false;
  return p;
}
//--------------------------------------------------------------------------------
InMemory::InMemory(const Parameter& p, Primary prim)
   : Architecture(adjustParams(p), prim, ArchType::InMemory), pageService{parameter, primary},
     logService{*InstanceStorageLogService::assemble(parameter, primary)} {
   // Updates
   Rate cpuUpdates = primary.n.cpu.getOps();
   double writesPerUpdate = parameter.groupCommit ? ((parameter.getRedoLogRecordSize() * 1.0) / InstanceStorage::MaxIOPSize) : divRoundUp(parameter.getRedoLogRecordSize(), InstanceStorage::MaxIOPSize);

   auto writeIops = primary.n.instanceStorage.getWriteOps();
   auto writeScale = writeIops / writesPerUpdate;
   updates = vmin(cpuUpdates, writeScale, parameter.requiredUpdateOps);

   // Lookups
   lookups = vmin(cpuUpdates - updates, parameter.requiredLookupOps);

   primary.logVolume = updates.rate * parameter.getRedoLogRecordSize();

   commitLatency = logService.getCommitLatency();
   opLatency = pageService.getOpLatency();
}
//--------------------------------------------------------------------------------
unique_ptr<InMemory> InMemory::assemble(const Parameter& p, const Node& n) {
   if (!n.instanceStorage && (p.requiredUpdateOps != Rate::zero)) return {};
   if (n.memory.getTotalSize() < p.datasetSize) return {};
   if (p.requiredOps() > n.cpu.getOps()) return {};

   // In-mem system only needs to persist redo log
   auto logWrites = p.requiredUpdateOps * (p.groupCommit ? (p.getRedoLogRecordSize() * 1.0) / InstanceStorage::MaxIOPSize : divRoundUp(p.getRedoLogRecordSize(), InstanceStorage::MaxIOPSize));

   if ((logWrites > Rate::zero) && (logWrites > n.instanceStorage.getWriteOps())) return {};
   if (p.getRequiredRedoLogStorage() > 0 && (p.getRequiredRedoLogStorage() > n.instanceStorage.getUsableSize())) return {};
   if (p.requiredOps() > n.cpu.getOps()) return {};

   Primary primary{p, n};

   return make_unique<InMemory>(p, primary);
}
//--------------------------------------------------------------------------------
Durability InMemory::getDurability() const {
  return logService.getDurability();
}
//--------------------------------------------------------------------------------
FailoverTime InMemory::getFailoverTime() const {
  auto& p = parameter;
  // start instance + download dataset + apply recovery
  // recovery depends on update rate
  // How often do we backup full database?
  FailoverTime download {p.datasetSize / primary.n.network.getReadLimit().rate};
  FailoverTime recovery{(1.0 * p.getRequiredLogStorage()) / 1000_mib};
  return Node::nodeSpinupTime + download + recovery;
}
//--------------------------------------------------------------------------------
