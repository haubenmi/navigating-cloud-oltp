#pragma once
#include "Resources.hpp"
#include <memory>
#include <optional>
//--------------------------------------------------------------------------------
struct Primary;
//--------------------------------------------------------------------------------
struct LogService {
   Parameter parameter;
   /// The node type for the page service
   LogService(const Parameter& p) : parameter{p} {
   }

   virtual ~LogService() = default;
   virtual std::string getDescription() const { return ""; }
   virtual Price getPrice() const { return Price::zero; }

   virtual bool isS3() const { return false; }
   virtual uint64_t getTotalSize() const { return 0; }
   virtual Latency getCommitLatency() const {
      return {};
   }
   virtual uint64_t getMaxIopSize() const { abort(); }
   virtual Rate getUpdateOps() const { abort(); }
   virtual Durability getDurability() const { abort(); }
};
//--------------------------------------------------------------------------------
struct NoopLogService : public LogService {
   NoopLogService(const Parameter& p) : LogService(p) {}
   std::string getDescription() const override { return "no-log"; }
};
//--------------------------------------------------------------------------------
struct InstanceStorageLogService : public LogService {
   Primary& primary;
   InstanceStorageAllotment storage;
   InstanceStorageLogService(const Parameter& p, Primary& prim, InstanceStorageAllotment inst);
   std::string getDescription() const override { return "inst-stor"; }
   Price getPrice() const override { return Price::zero; }
   Latency getCommitLatency() const override { return InstanceStorage::writeLatency; }
   uint64_t getMaxIopSize() const override { return InstanceStorage::MaxIOPSize; }
   Rate getUpdateOps() const override;
   Durability getDurability() const override;

   static std::unique_ptr<InstanceStorageLogService> assemble(const Parameter& p, Primary& prim);
};
//--------------------------------------------------------------------------------
struct EBSLogService : public LogService {
   Primary& primary;
   EBSAllotment ebs;
   EBSLogService(const Parameter& p, Primary& prim, EBSAllotment ebs);
   static std::unique_ptr<EBSLogService> assemble(const Parameter& p, Primary& prim, EBS::Type t);
   Price getPrice() const override;
   uint64_t getMaxIopSize() const override { return EBS::maxIopSize; }
   Rate getUpdateOps() const override;
   Durability getDurability() const override;
   std::string getDescription() const override;
};
//--------------------------------------------------------------------------------
struct Ec2LogService : public LogService {
   /// The node type for the page service
   static constexpr double logServiceReplication = 1.0;

   Node logNode;
   double logNodeFraction;
   unsigned targets;
   EBSAllotment logEBSDevice;

   public:
   Ec2LogService(const Parameter& p, const Node& logNode, EBSAllotment ebs, double scale, unsigned logTargets)
      : LogService{p}, logNode{logNode}, logNodeFraction{scale}, targets{logTargets}, logEBSDevice{ebs} {}
   // EBS device is attributed to the primary
   Price getPrice() const override { return logNodeFraction * logNode.price; }

   Rate getUpdateOps() const override;

   static double getReplication() { return logServiceReplication; }

   static std::unique_ptr<Ec2LogService> assemble(const Parameter& p, Primary& prim, const Node& logNode, unsigned replication);
   static double computeScale(const Parameter& p, const Node& logNode, unsigned logTargets);

   std::string getDescription() const override;
   Durability getDurability() const override;
   Latency getCommitLatency() const override;

   uint64_t getMaxIopSize() const override;
};
//--------------------------------------------------------------------------------
