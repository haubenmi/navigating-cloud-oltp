#include "DynamicArchitecture.hpp"
#include "infra/Math.hpp"
//--------------------------------------------------------------------------------
using namespace std;
using namespace infra;
//--------------------------------------------------------------------------------
Dynamic::Dynamic(const Parameter& p, unique_ptr<Primary> prim, unique_ptr<PageService> pageS, unique_ptr<LogService> logS)
   : Architecture{p, *prim, ArchType::Dynamic}, primary{std::move(prim)}, pageService{std::move(pageS)}, logService{std::move(logS)} {
   // Updates
   auto cacheHitOps = primary->getCacheHitOps();
   auto pageWritesPerOp = primary->probEvictDirtyPageFromCache();
   auto pageReadsPerOp = primary->probCacheMiss();
   auto availablePageReadOps = pageService->getPageReadOps();
   auto availablePageWriteOps = pageService->getPageWriteOps();

   updates = vmin(cacheHitOps, availablePageReadOps / pageReadsPerOp, availablePageWriteOps / pageWritesPerOp, logService->getUpdateOps(), parameter.requiredUpdateOps);

   // Lookups
   availablePageReadOps -= updates * pageReadsPerOp;
   availablePageWriteOps -= updates * pageWritesPerOp;
   auto cacheHitOpsForLookups = primary->getCacheHitOps(updates);
   lookups = vmin(cacheHitOpsForLookups,
                  availablePageReadOps / pageReadsPerOp,
                  availablePageWriteOps / pageWritesPerOp,
                  parameter.requiredLookupOps);

   secLookups = vmin(lookups * secondaries.availableForLookups(), parameter.requiredLookupOps - lookups);


   commitLatency = logService->getCommitLatency();
   opLatency = Latency::combine({{primary->probCacheMiss(), pageService->getOpLatency()}, {primary->probCacheHit(), primary->getCacheHitLatency()}});
}
//--------------------------------------------------------------------------------
vector<unique_ptr<Dynamic>> Dynamic::assemble(const Parameter& p2, const Node& n,  [[maybe_unused]] const vector<Node>& pageNodes, [[maybe_unused]] const vector<Node>& logNodes) {
   vector<unique_ptr<Dynamic>> results;

   // So far: no secondaries, primary never uses rbpex

   auto generateLogServices = [&](const Parameter& p, auto& refresh, auto& primary) {
      if (auto pageService = refresh()) {
         if (auto logService = InstanceStorageLogService::assemble(p, *primary)) {
            results.push_back(make_unique<Dynamic>(p, std::move(primary), std::move(pageService), std::move(logService)));
         }
         using T = EBS::Type;
         for (auto t : {T::gp3, T::gp2, T::io2, T::io1}) {
            pageService = refresh();
            if (auto logService = EBSLogService::assemble(p, *primary, t); logService) {
               results.push_back(make_unique<Dynamic>(p, std::move(primary), std::move(pageService), std::move(logService)));
            }
         }

         // Aurora storage servers
         for (auto& storageNode : pageNodes) {
            pageService = refresh();
            auto logService = CombinedPageServiceLog::assemble(p, *primary, storageNode, Latency() /*in-mem, does not do any page lookups anyway*/);
            results.push_back(make_unique<Dynamic>(p, std::move(primary), std::move(pageService), std::move(logService)));
         }
         // Socrates log service
         unsigned pageServerReplication = 1;
         for (auto& logNode : logNodes) {
            pageService = refresh();
            if (auto logService = Ec2LogService::assemble(p, *primary, logNode, pageServerReplication)) {
               results.push_back(make_unique<Dynamic>(p, std::move(primary), std::move(pageService), std::move(logService)));
            }
         }
      }
   };

   auto generatePageServices = [&](const Parameter& p3, bool useRbpexOnPrimary) {
      // In-mem
      Parameter p = p3;
      unique_ptr<Primary> primary;
      auto makePrimary = [&]() { return Primary::assemble(p, n, useRbpexOnPrimary); };
      p.walIncludesUndo = false;
      auto refreshInMem = [&]() -> unique_ptr<PageService> {
         primary = makePrimary();
         if (!primary) return nullptr;
         primary->logVolume = p.requiredUpdateOps.rate * p.getLogRecordSize();
         return InMemoryPageService::assemble(p, *primary);
      };
      generateLogServices(p, refreshInMem, primary);

      // Store the DB on instance storage and use a buffer manager
      p.walIncludesUndo = true;
      auto refreshInstanceStorage = [&]() -> unique_ptr<PageService> {
         primary = makePrimary();
         if (!primary) return nullptr;
         primary->logVolume = p.requiredUpdateOps.rate * p.getLogRecordSize();
         return InstanceStoragePageService::assemble(p, *primary);
      };
      generateLogServices(p, refreshInstanceStorage, primary);

      // Store the db on EBS, otherwise same as before
      p.walIncludesUndo = true;
      using T = EBS::Type;
      for (auto t : {T::gp3, T::gp2, T::io2, T::io1}) {
         auto refreshEBS = [&]() -> unique_ptr<PageService> {
            primary = makePrimary();
            if (!primary) return nullptr;
            primary->logVolume = p.requiredUpdateOps.rate * p.getLogRecordSize();
            return EBSPageService::assemble(p, *primary, t);
         };
         generateLogServices(p, refreshEBS, primary);
      }

      // Store the db on page servers; since they need the redo log, stream it to them from the primary.
      // If the log service supports it, it can also do that
      p.walIncludesUndo = false;
      // With RBPEX
      for (auto& pageNode : pageNodes) {
         auto refreshPageService = [&]() -> unique_ptr<PageService> {
            primary = makePrimary();
            if (!primary) return nullptr;
            primary->logVolume = p.requiredUpdateOps.rate * p.getLogRecordSize();
            auto primLatency = Latency::deduce(p.requiredOpLatency, {{primary->probCacheHit(), primary->getCacheHitLatency()}});
            return Ec2PageService::assemble(p, *primary, pageNode, primLatency, true /*rbpex*/);
         };
         generateLogServices(p, refreshPageService, primary);
      }
      // Without RBPEX
      for (auto& pageNode : pageNodes) {
         auto refreshPageService = [&]() -> unique_ptr<PageService> {
            primary = makePrimary();
            if (!primary) return nullptr;
            primary->logVolume = p.requiredUpdateOps.rate * p.getLogRecordSize();
            auto primLatency = Latency::deduce(p.requiredOpLatency, {{primary->probCacheHit(), primary->getCacheHitLatency()}});
            return Ec2PageService::assemble(p, *primary, pageNode, primLatency, false /*rbpex*/);
         };
         generateLogServices(p, refreshPageService, primary);
      }

      // Aurora style
      p.walIncludesUndo = false;
      for (auto& pageNode : pageNodes) {
         auto refreshStorageService = [&]() {
            primary = makePrimary();
            if (!primary) return;
            primary->logVolume = p.requiredUpdateOps.rate * p.getLogRecordSize();
            auto primLatency = Latency::deduce(p.requiredOpLatency, {{primary->probCacheHit(), primary->getCacheHitLatency()}});
            auto pageService =  CombinedPageServiceLog::assemble(p, *primary, pageNode, primLatency);
            assert(pageService);
            //            if (!pageService) return;
            auto logService = make_unique<CombinedPageServiceLogWrapper>(*pageService);
            results.push_back(make_unique<Dynamic>(p, std::move(primary), std::move(pageService), std::move(logService)));
         };
         refreshStorageService();
      }
   };
   for (unsigned i = 0; i <= p2.maxSecondaries;++i) {
      auto p = p2;
      p.numSecondaries = i;
      generatePageServices(p, true);
      generatePageServices(p, false);
   }
   return results;
}
   //--------------------------------------------------------------------------------
