#include "infra/ArgumentParser.hpp"
#include "infra/File.hpp"
#include "infra/Parser.hpp"
#include "ArchitectureBuilder.hpp"
#include "Metric.hpp"
#include "MetricRegistry.hpp"
#include "Metrics.hpp"
#include <chrono>
#include <cstdio>
#include <exception>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
//--------------------------------------------------------------------------------
using namespace std;
using namespace infra;
//--------------------------------------------------------------------------------
struct CloudCalcArgs : public ArgumentParser {
   OptionalArgument<string> instancesCSV{this, ShortName{"c"}, "instances-csv", "instances csv path", "./instances.csv"};
   OptionalArgument<uint64_t> datasetSize{this, "datasize", "the size of the data set (in GB)", 100};
   OptionalArgument<double> dataBloat{this, "data-bloat", "the factor how much the data volume is larger in storage", 1.5};
   OptionalArgument<double> usableMemory{this, "usable-memory", "the factor how much memory of the instance can be used for the buffer pool", 0.9};
   OptionalArgument<double> networkOverhead{this, "network-overhead", "the factor how much overhead the network traffic introduces", 1.0};
   OptionalArgument<uint64_t> transactions{this, "transactions", "the number of operations", 10000};
   OptionalArgument<double> updateRatio{this, "update-ratio", "the update ratio", 0.3};
   OptionalArgument<double> lookupZipf{this, "lookup-zipf", "the skew of the keys of the lookup", 0.0};
   OptionalArgument<uint64_t> maxReplicas{this, "max-replicas", "the max number of replicas for which to build architectures", 3};
   OptionalArgument<uint64_t> minReplicas{this, "min-replicas", "the max number of replicas for which to build architectures", 0};
   OptionalArgument<uint64_t> pageSize{this, "pagesize", "the size of a single data page", 4096};
   OptionalArgument<uint64_t> cpuCost{this, "cpu-cost", "the in-memory cost of a single operation", 4000};
   OptionalArgument<uint64_t> tupleSize{this, "tuplesize", "the size of a single tuple", 68};
   OptionalArgument<uint64_t> requiredOpLatency{this, "latency", "the required latency (in ns) for an operation", 9999999999};
   OptionalArgument<uint32_t> requiredDurability{this, "durability", "the required durability for an architecture", 0};
   OptionalArgument<unsigned> pageServerReplication{this, "page-server-replication", "the number of page servers (if used) on which each page is replicated", 2};
   OptionalArgument<bool> groupCommit{this, "group-commit", "let the model use group commit", true};
   OptionalArgument<bool> indexOnlyTables{this, "index-only-tables", "let the model use index-only tables", true};
   OptionalArgument<bool> deployAcrossAZ{this, "inter-az", "let the model try to distribute instances across AZs", false};

   OptionalArgument<string> sortOrder{this, "sort", "the category on which to sort", "TotalPrice"};
   OptionalArgument<string> priceUnit{this, "priceunit", "print the prices by this unit", "hour"};
   OptionalArgument<string> instanceFilter{this, "instances", "filter the instances by this", ""};
   OptionalArgument<string> architectures{this, "architectures", "consider these architectures, empty=all", ""};
   OptionalArgument<string> excludedArchitectures{this, "excludes", "exclude these architectures", "dynamic"};
   OptionalArgument<string> csvDelimiter{this, "delimiter", "delimiter for csv mode", ","};
   OptionalArgument<uint64_t> trunc{this, "trunc", "truncate the results, but keep at least this much from each arch", 10};
   OptionalArgument<bool> filter{this, "filter", "filter the results", true};
   OptionalArgument<bool> csvFormat{this, "csv", "print in csv format", false};
   OptionalArgument<bool> showHidden{this, "show-hidden", "print metrics that are by default hidden", false};
   OptionalArgument<bool> hideCosts{this, "hide-costs", "hide the costs", false};
   OptionalArgument<bool> hideLookups{this, "hide-lookups", "hide the lookups", false};
   OptionalArgument<bool> hideUpdates{this, "hide-updates", "hide the updates", false};
   OptionalArgument<bool> terse{this, "terse", "hide the unimportant metrics", false};

   OptionalArgument<double> ec2Discount{this, "ec2-discount", "The discount on EC2 (but not EBS,S3 etc.) we assume due to reserved instance savings etc.", 0.5};

   OptionalArgument<double> intraAZLatency{this, "intra-az-latency", "The assumed latency between two ec2 machines in the same AZ", 0.5};
   OptionalArgument<double> interAZLatency{this, "inter-az-latency", "The assumed latency between two ec2 machines in different AZs in the same region", 1.0};
};
//--------------------------------------------------------------------------------
int main(int argc, char** argv) {
   CloudCalcArgs args;
   if (!args.parse(argc, argv)) {
      cerr << args.getHelp() << "\n";
      exit(1);
   }

   BinaryUnitInterpreter::machineReadable = args.csvFormat.get();
   DecimalUnitInterpreter::machineReadable = args.csvFormat.get();
   Latency::machineReadable = args.csvFormat.get();
   Price::machineReadable = args.csvFormat.get();
   Durability::machineReadable = args.csvFormat.get();
   FailoverTime::machineReadable = args.csvFormat.get();

   if (args.priceUnit.get() == "hour") {
      Price::timeunitForPrint = Timeunit::Hour;
   } else if (args.priceUnit.get() == "day") {
      Price::timeunitForPrint = Timeunit::Day;
   } else if (args.priceUnit.get() == "month") {
      Price::timeunitForPrint = Timeunit::Month;
   } else if (args.priceUnit.get() == "year") {
      Price::timeunitForPrint = Timeunit::Year;
   } else {
      cerr << "Invalid price unit: " << args.priceUnit.get() << "\n";
      exit(1);
   }


   VantageCSV vantageCSV;
   try {
      infra::File data{args.instancesCSV.get(), File::AccessMode::ReadOnly};
      data.open(OpenMode::Open);
      auto input = data.readWholeFile();
      CSVReader reader{input};
      vantageCSV.parse(reader);

   } catch (const exception& e) {
      cerr << e.what();
      exit(1);
   }
   auto datasetSizeInBytes = 1024ull * 1024 * 1024 * args.datasetSize;

   if (args.lookupZipf != 0.0 && args.updateRatio > 0) {
     cerr << "Error! Cannot specify a lookup zipf when there are also updates\n";
     return 1;
   }
   auto updates = args.transactions * args.updateRatio;
   auto lookups = args.transactions - updates;
   Parameter p{
      .datasetSize = datasetSizeInBytes,
      .dataBloat = args.dataBloat,
      .usableMemory = args.usableMemory,
      .networkOverhead = args.networkOverhead,
      .requiredLookupOps = Rate::secondly(lookups),
      .lookupZipf = args.lookupZipf,
      .requiredUpdateOps = Rate::secondly(updates),
      .tupleSize = args.tupleSize,
      .pageSize = args.pageSize,
      .cpuCost = args.cpuCost,
      .minSecondaries = static_cast<unsigned>(args.minReplicas.get()),
      .maxSecondaries = static_cast<unsigned>(args.maxReplicas.get()),
      .intraAZLatency = args.intraAZLatency,
      .interAZLatency = args.interAZLatency,
      .ec2Discount = args.ec2Discount,
      .pageServerReplication = args.pageServerReplication,
      .groupCommit = args.groupCommit,
      .deployAcrossAZ = args.deployAcrossAZ,
      .indexOnlyTables = args.indexOnlyTables,
      .requiredOpLatency = Latency{nanoseconds(args.requiredOpLatency.get())},
      .requiredDurability = Durability{args.requiredDurability, nines},
   };

   if(p.minSecondaries > p.maxSecondaries) {
     cerr << "min secondaries must be smaller than max secondaries";
     exit(1);
   }

   auto archs = infra::Parser::split(args.architectures.get(), ',');
   auto excludes = infra::Parser::split(args.excludedArchitectures.get(), ',');
   if (archs.size() == 1 && archs[0] == "") archs.clear();
   ArchitectureBuilder builder{vantageCSV, p, args.instanceFilter.get(), archs, excludes};

   MetricRegistry registry{args.csvFormat, args.showHidden,args.csvDelimiter};
   registry.add<IdMetric>();
   registry.add<TypeMetric>();
   registry.add<PrimaryMetric>();
   if (!args.terse) registry.add<CPUVendorMetric>();
   registry.add<StorageMetric>();
   if (!args.terse) registry.add<StorageDevice>();
   registry.add<LogServiceMetric>();
   registry.add<SecondaryMetric>();
   registry.add<DurabilityMetric>(p.requiredDurability);
   registry.add<OpLatencyMetric>(p.requiredOpLatency);
   if (!args.terse) registry.add<CommitLatencyMetric>();
   registry.add<TotalPrice>();
   registry.add<PrimaryPrice>();
   registry.add<EBSPrice>();
   registry.add<SecondariesPrice>();
   registry.add<LogServicePrice>();
   registry.add<PageServicePrice>();
   if (!args.terse) registry.add<S3Price>();
   registry.add<NetworkPrice>();
   if (!args.terse) registry.add<DatasetSize>(p.getDataSize());
   registry.add<PrimaryBufferCache>();
   registry.add<PrimaryBufferCacheHitrate>();
   registry.add<StorageCapacity>();
   registry.add<PrimaryRandomLookupTx>();
   registry.add<SecondariesRandomLookupTx>();
   registry.add<RandomLookupTx>(p.requiredLookupOps);
   registry.add<RandomUpdateTx>(p.requiredUpdateOps);
   if (!args.terse) registry.add<PageReadVolume>();
   if (!args.terse) registry.add<PageWriteVolume>();
   registry.add<NetworkInVolume>();
   registry.add<NetworkOutVolume>();
   registry.add<LogVolume>();
   if (!args.terse) registry.add<InterAZTraffic>();

   registry.printHeader(args.csvFormat ? cout : cerr);
   for (auto& a : builder.getArchitectures()) {
     registry.insert(*a);
   }
   if (args.filter.get()) {
     registry.filter();
   }
   if (!args.sortOrder.get().empty()) {
     registry.sortAndTrunc(args.sortOrder.get(), args.trunc.get());
   }
   registry.print(cout);
}
//--------------------------------------------------------------------------------
