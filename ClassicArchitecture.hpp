#pragma once
#include "Architecture.hpp"
//--------------------------------------------------------------------------------
struct Classic : public Architecture {
   InstanceStoragePageService pageService;
   InstanceStorageLogService logService;

   Rate lookups = Rate::zero;
   Rate updates = Rate::zero;

   const PageService& getPageService() const override { return pageService; }
   const LogService& getLogService() const override { return logService; }

   Classic(const Parameter& p, Primary prim);

   uint64_t getS3Storage() const override { return 0; }
   Rate getS3GETRate() const override { return Rate::zero; }
   Rate getS3PUTRate() const override { return Rate::zero; }

   uint64_t getInterAZTraffic() const override { return 0; }

   Rate getPrimaryRandomLookupTx() const override { return lookups; }
   Rate getRandomUpdateTx() const override { return updates; }

   Rate getSecondariesRandomLookupTx() const override { return Rate::zero; }
   Durability getDurability() const override;
   FailoverTime getFailoverTime() const override;

   static std::unique_ptr<Classic> assemble(const Parameter& p, const Node& n);
};
//--------------------------------------------------------------------------------
