#pragma once
#include "Resources.hpp"
#include "LogService.hpp"
#include <memory>
#include <string_view>
//--------------------------------------------------------------------------------
struct Primary;
//--------------------------------------------------------------------------------
struct PageService {
   Parameter parameter;

   PageService(const Parameter& p);
   PageService(const PageService& p);
   virtual ~PageService() = default;
   virtual std::string getDescription() const { return ""; }
   virtual Price getPrice() const = 0;
   virtual uint64_t getTotalSize() const { return 0; }

   virtual uint64_t getWriteVolume() const { return 0; }
   virtual uint64_t getReadVolume() const { return 0; }
  //   virtual Rate getWriteLimit() const { return Rate::zero; }
   virtual bool isDisk() const { return false; }
   virtual bool isS3() const { return false; }
   virtual Latency getOpLatency() const { abort(); }
   virtual Rate getPageReadOps() const { abort(); }
   virtual Rate getPageWriteOps() const { abort(); }
   virtual std::string getDeviceType() const { return ""; }
   virtual bool containsLogService() const { return false; }
};
//--------------------------------------------------------------------------------
struct NoopPageService : public PageService {
   NoopPageService(const Parameter& p) : PageService(p) {}

   std::string getDescription() const override { return "no-p"; }
   Price getPrice() const override { return Price::zero; }
   uint64_t getTotalSize() const override { return 0; }
   uint64_t getWriteVolume() const override { return 0; }
   uint64_t getReadVolume() const override { return 0; }
};
//--------------------------------------------------------------------------------
struct InMemoryPageService : public PageService {
  Primary& primary;

  InMemoryPageService(const Parameter& p, Primary& prim) : PageService(p), primary{prim} {}
  static std::unique_ptr<InMemoryPageService> assemble(const Parameter& p, Primary& prim);
  //  static bool isCompatible(const Parameter& p, const Primary& prim);
  std::string getDescription() const override { return "in-mem"; }
  Price getPrice() const override { return Price::zero; }
  // We checked before that the data fits into memory
  uint64_t getTotalSize() const override { return 0; }
  // We model this via CPU limtis
  Rate getPageReadOps() const override { return Rate::unlimited; }
  Rate getPageWriteOps() const override { return Rate::unlimited; }
  uint64_t getWriteVolume() const override { return 0; }
  uint64_t getReadVolume() const override { return 0; }
  Latency getOpLatency() const override { return Memory::readLatency; }
};
//--------------------------------------------------------------------------------
struct InstanceStoragePageService : public PageService {
   Primary& primary;
   InstanceStorageAllotment storage;
   /// Ctor
   InstanceStoragePageService(const Parameter& p, Primary& prim, InstanceStorageAllotment);
   bool isDisk() const override { return true; }
   // Price already included in primary/secondaries
   Price getPrice() const override { return Price::zero; }
   std::string getDescription() const override;
   uint64_t getTotalSize() const override;
   uint64_t getWriteVolume() const override { return storage.writes.nextInt() * parameter.pageSize; }
   uint64_t getReadVolume() const override { return storage.reads.nextInt() * parameter.pageSize; }
   Rate getPageReadOps() const override { return storage.reads; }
   Rate getPageWriteOps() const override { return storage.writes; }
   std::string getDeviceType() const override;
   Latency getOpLatency() const override;

   static std::unique_ptr<InstanceStoragePageService> assemble(const Parameter& p, Primary& prim);
};
//--------------------------------------------------------------------------------
struct EBSPageService : public PageService {
   Primary& primary;
   // uint64_t readVolume;
   // uint64_t writeVolume;
   EBSAllotment ebs;
   Rate reads;
   Rate writes;
   /// Ctor
   EBSPageService(const Parameter& p, Primary& prim, EBSAllotment ebs, Rate reads, Rate writes);
   static std::unique_ptr<EBSPageService> assemble(const Parameter& p, Primary& prim, EBS::Type t);
   bool isDisk() const override { return true; }
   uint64_t getWriteVolume() const override { return ebs.iops.nextInt() * parameter.pageSize; }
   uint64_t getReadVolume() const override { return ebs.iops.nextInt() * parameter.pageSize; }
   Rate getPageReadOps() const override { return reads; }
   Rate getPageWriteOps() const override { return writes; }
   std::string getDescription() const override;
   Price getPrice() const override;
   uint64_t getTotalSize() const override;
   std::string getDeviceType() const override;
   Latency getOpLatency() const override { return EBS::readLatency; }
};
//--------------------------------------------------------------------------------
struct S3PageService : public PageService {
   /// Needed for the network bandwidth
   Primary& primary;
   S3PageService(const Parameter& p, Primary& primary) : PageService(p), primary{primary} {}

   std::string getDescription() const override { return "s3-p"; }
   Price getPrice() const override { return Price::zero; }
   uint64_t getTotalSize() const override { return 0; }

   bool isS3() const override { return true; }
   std::string getDeviceType() const override { return "s3"; }
};
//--------------------------------------------------------------------------------
struct Ec2PageService : public PageService {
   /// The node type for the page service
   Node pageNode;
   double pageNodeFraction;
   bool useRbpex;

   public:
   Ec2PageService(const Parameter& p, Node pageNode, double pageNodeFraction, bool useRbpex) : PageService{p}, pageNode{pageNode}, pageNodeFraction{pageNodeFraction}, useRbpex{useRbpex} {}
   Price getPrice() const override { return pageNodeFraction * pageNode.price; }
  uint64_t getTotalSize() const override { return pageNodeFraction * (pageNode.instanceStorage.getUsableSize() + (useRbpex ? pageNode.memory.getTotalSize() : 0)); }
   std::string getDescription() const override;
   std::string getDeviceType() const override { return "ec2"; }

   Rate getPageReadOps() const override;
   // No write back of materialized pages, thus unlimited
   Rate getPageWriteOps() const override { return Rate::unlimited; }

   double getPageNodeCacheMiss() const;
   Latency getOpLatency() const override;

   static std::unique_ptr<Ec2PageService> assemble(const Parameter& p, Primary& prim, Node pageNode, Latency targetLatency, unsigned replication, bool userbpex = true);
};
//--------------------------------------------------------------------------------
// Models Aurora
struct CombinedPageServiceLog : public PageService, public LogService {
   static constexpr unsigned replication = 6;

   Node n;
   double fraction;

   CombinedPageServiceLog(const Parameter& p, Node n, double frac) : PageService(p), LogService(p), n{n}, fraction{frac} {}
   void init(double primaryCacheMiss, Latency targetOpLatency);
   bool containsLogService() const override { return true; }
   uint64_t getTotalSize() const override { return fraction * n.instanceStorage.getUsableSize(); }
   Price getPrice() const override { return fraction * n.price; }
   std::string getDescription() const override;

   unsigned getReplication() const { return replication; }

   uint64_t getMaxIopSize() const override { return InstanceStorage::MaxIOPSize; }
   Latency getCommitLatency() const override;
   Latency getOpLatency() const override;
   double getPageNodeCacheMiss() const;
   Durability getDurability() const override;
   Rate getUpdateOps() const override;
   Rate getPageReadOps() const override;
  // No write back of materialized pages, thus unlimited
   Rate getPageWriteOps() const override { return Rate::unlimited; }

   static std::unique_ptr<CombinedPageServiceLog> assemble(const Parameter& p, Primary& prim, Node pageNode, Latency targetOpLatency);
};
//--------------------------------------------------------------------------------
struct CombinedPageServiceLogWrapper : public LogService {
   const CombinedPageServiceLog& storage;

   CombinedPageServiceLogWrapper(const CombinedPageServiceLog& storage) : LogService(storage.LogService::parameter), storage{storage} {}

   //  const CombinedPageServiceLog* operator->() { return &storage; }
   uint64_t getMaxIopSize() const override { return storage.getMaxIopSize(); }
   Price getPrice() const override { return Price::zero; }
   Latency getCommitLatency() const override { return storage.getCommitLatency(); }
   Rate getUpdateOps() const override { return storage.getUpdateOps(); }
   Durability getDurability() const override { return storage.getDurability(); }
};
//--------------------------------------------------------------------------------
