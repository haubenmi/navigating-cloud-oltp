#pragma once
#include "Architecture.hpp"
#include "PageService.hpp"
//--------------------------------------------------------------------------------
struct SocratesLike : public Architecture {
   Ec2PageService pageService;
   Ec2LogService logService;

   Rate lookups = Rate::zero;
   Rate updates = Rate::zero;
   Rate secLookups = Rate::zero;
   uint64_t interAZTraffic = 0;

   const PageService& getPageService() const override { return pageService; }
   const LogService& getLogService() const override { return logService; }

    // Durability in Socrates is limited by the log landing zone, which lives on a storage volume
    // The log is later moved to xstore, which is the S3 equivalent, so has clearly higher durability.
   static Durability durability;

   /// Ctor
   SocratesLike(const Parameter& p, const Primary& n, const Node& pageNode, std::unique_ptr<Ec2LogService> log);

   uint64_t getS3Storage() const override { return parameter.getDataSize(); }
   Rate getS3GETRate() const override { return Rate::zero; }
   Rate getS3PUTRate() const override { return Rate::zero; }

   Rate getPrimaryRandomLookupTx() const override { return lookups; }
   Rate getSecondariesRandomLookupTx() const override { return secLookups; }
   Rate getRandomUpdateTx() const override { return updates; }

   uint64_t getInterAZTraffic() const override { return interAZTraffic; }

   Durability getDurability() const override { return durability; }
   FailoverTime getFailoverTime() const override;

   static std::unique_ptr<SocratesLike> assemble(const Parameter& p, const Node& n, const Node& page, const Node& log, bool useRBPex = true);
};
//--------------------------------------------------------------------------------
