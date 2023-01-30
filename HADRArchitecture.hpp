#pragma once
#include "Architecture.hpp"
//--------------------------------------------------------------------------------
struct HADR : public Architecture {
   InstanceStoragePageService pageService;
   InstanceStorageLogService logService;

   Rate lookups = Rate::zero;
   Rate updates = Rate::zero;
   Rate secLookups = Rate::zero;

   const PageService& getPageService() const override { return pageService; }
   const LogService& getLogService() const override { return logService; }

   HADR(const Parameter& p, Primary prim);

   uint64_t getS3Storage() const override { return 0; }
   Rate getS3GETRate() const override { return Rate::zero; }
   Rate getS3PUTRate() const override { return Rate::zero; }

   uint64_t getInterAZTraffic() const override;

   Rate getPrimaryRandomLookupTx() const override { return lookups; }
   Rate getSecondariesRandomLookupTx() const override { return secLookups; }
   Rate getRandomUpdateTx() const override { return updates; }

   Durability getDurability() const override;
   FailoverTime getFailoverTime() const override;

   static std::unique_ptr<HADR> assemble(const Parameter& p, Node n);
};
//--------------------------------------------------------------------------------
