#pragma once
#include "Architecture.hpp"
#include "PageService.hpp"
#include "LogService.hpp"
//--------------------------------------------------------------------------------
// Dimensions that can be dynamic:
// dyn bp ext yes/no
// storage: inmem, instance, ebs, storageservers, pageservers, s3?
// log: instance, ebs, storageservers, logservice (isn't it just ebs?)
// replicas: #no
// answer question like: would aurora benefit from pageservers? In-Mem with instance storage or EBS?
// multiply it out
struct Dynamic : public Architecture {
   //  Primary primary;
   std::unique_ptr<Primary> primary;
   std::unique_ptr<PageService> pageService;
   std::unique_ptr<LogService> logService;

   Rate lookups = Rate::zero;
   Rate updates = Rate::zero;
   Rate secLookups = Rate::zero;

  Dynamic(const Parameter& p, std::unique_ptr<Primary> n, std::unique_ptr<PageService> pageService, std::unique_ptr<LogService> logService);
   const PageService& getPageService() const override { return *pageService; }
   const LogService& getLogService() const override { return *logService; }

   uint64_t getS3Storage() const override {
      return (pageService->isS3() ? pageService->getTotalSize() : 0) + (logService->isS3() ? logService->getTotalSize() : 0);
  }
  uint64_t getInterAZTraffic() const override { return 0; }
  Rate getS3GETRate() const override { return Rate::zero; }
  Rate getS3PUTRate() const override { return Rate::zero; }
  Durability getDurability() const override { return logService->getDurability(); }
  Rate getPrimaryRandomLookupTx() const override { return lookups; }
  Rate getSecondariesRandomLookupTx() const override { return secLookups; }
  Rate getRandomUpdateTx() const override { return updates; }
  //  Secondaries secondaries;

  static std::vector<std::unique_ptr<Dynamic>> assemble(const Parameter& p, const Node& n, const std::vector<Node>& page, const std::vector<Node>& log);
};
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
