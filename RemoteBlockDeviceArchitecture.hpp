#pragma once
#include "Architecture.hpp"
//--------------------------------------------------------------------------------
class RemoteBlockDevice : public Architecture {
   EBSPageService pageService;
   EBSLogService log;

   Rate lookups = Rate::zero;
   Rate updates = Rate::zero;

   public:
   RemoteBlockDevice(Parameter p, Primary n, EBSAllotment t);

   const PageService& getPageService() const override { return pageService; }
   const LogService& getLogService() const override { return log; }

   uint64_t getS3Storage() const override { return 0; }
   Rate getS3GETRate() const override { return Rate::zero; }
   Rate getS3PUTRate() const override { return Rate::zero; }

   Durability getDurability() const override;
   FailoverTime getFailoverTime() const override;

   Rate getPrimaryRandomLookupTx() const override { return lookups; }
   Rate getRandomUpdateTx() const override { return updates; }

   uint64_t getInterAZTraffic() const override { return 0; }

   static std::unique_ptr<RemoteBlockDevice> assemble(const Parameter& p, Node n, EBS::Type t);
};
//--------------------------------------------------------------------------------
