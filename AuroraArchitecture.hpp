#pragma once
#include "Architecture.hpp"
//--------------------------------------------------------------------------------
struct AuroraLike : public Architecture {
   CombinedPageServiceLog storageService;

   // Paper Section 4.2.4
   static constexpr unsigned maxSecondaries = 15;
   static constexpr unsigned dataReplication = 3;
   static constexpr unsigned logReplication = 6;

   Rate lookups = Rate::zero;
   Rate updates = Rate::zero;
   Rate secLookups = Rate::zero;

   uint64_t interAZTraffic = 0;

   const PageService& getPageService() const override { return storageService; }
   const LogService& getLogService() const override { return storageService; }
   /// Ctor
   AuroraLike(Parameter p, Node n, Node storageNode);
   uint64_t getS3Storage() const override { return 0; }
   Rate getS3GETRate() const override { return Rate::zero; }
   Rate getS3PUTRate() const override { return Rate::zero; }

   Rate getPrimaryRandomLookupTx() const override { return lookups; }
   Rate getSecondariesRandomLookupTx() const override { return secLookups; }
   Rate getRandomUpdateTx() const override { return updates; }

   Durability getDurability() const override;

   uint64_t getInterAZTraffic() const override { return interAZTraffic; }

   FailoverTime getFailoverTime() const override;

   static std::unique_ptr<AuroraLike> assemble(const Parameter& p, const Node& n, const Node& s);
};
//--------------------------------------------------------------------------------
