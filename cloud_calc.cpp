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
   OptionalArgument<string> vantageApiToken{this, "token", "vantage api token"};
   OptionalArgument<string> vantageCSV{this, ShortName{"c"}, "vantage-csv", "vantage csv path", "./vantage.csv"};
   OptionalArgument<uint64_t>
   datasetSize{this, "datasize", "the size of the data set (in GB)", 100};
  //   OptionalArgument<uint64_t> lookupOps{this, "lookups", "the minimal number of lookup transactions per second that are required to execute the workload", 10000};
   OptionalArgument<uint64_t> transactions{this, "transactions", "the number of operations", 10000};
   OptionalArgument<double> updateRatio{this, "update-ratio", "the update ratio", 0.3};
   OptionalArgument<double> lookupZipf{this, "lookup-zipf", "the skew of the keys of the lookup", 0.0};
   //   OptionalArgument<uint64_t> updateOps{this, "updates", "the minimal number of update transactions per second that are required to execute the workload", 1000};
   OptionalArgument<uint64_t> maxReplicas{this, "max-replicas", "the max number of replicas for which to build architectures", 3};
   OptionalArgument<uint64_t> minReplicas{this, "min-replicas", "the max number of replicas for which to build architectures", 0};
   OptionalArgument<uint64_t> pageSize{this, "pagesize", "the size of a single data page", 4096};
   OptionalArgument<uint64_t> tupleSize{this, "tuplesize", "the size of a single tuple", 52};
   OptionalArgument<uint64_t> requiredOpLatency{this, "latency", "the required latency (in ns) for an operation", 9999999999};
   OptionalArgument<uint32_t> requiredDurability{this, "durability", "the required durability for an architecture", 0};
   OptionalArgument<bool> groupCommit{this, "group-commit", "let the model use group commit", true};
   OptionalArgument<bool> deployAcrossAZ{this, "inter-az", "let the model try to distribute instances across AZs", false};

   OptionalArgument<string> sortOrder{this, "sort", "the category on which to sort", "TotalPrice"};
   OptionalArgument<string> priceUnit{this, "priceunit", "print the prices by this unit", "hour"};
   OptionalArgument<string> instanceFilter{this, "instances", "filter the instances by this", ""};
   OptionalArgument<string> architectures{this, "architectures", "consider these architectures, empty=all", ""};
   OptionalArgument<string> excludedArchitectures{this, "excludes", "exclude these architectures", "dynamic"};
   OptionalArgument<string> csvDelimiter{this, "delimiter", "delimiter for csv mode", ","};
   OptionalArgument<bool> trunc{this, "trunc", "truncate the results, but keep at least ten from each arch", true};
   OptionalArgument<bool> filter{this, "filter", "filter the results", true};
   OptionalArgument<bool> csvFormat{this, "csv", "print in csv format", false};
   OptionalArgument<bool> showHidden{this, "show-hidden", "print metrics that are by default hidden", false};
   OptionalArgument<bool> hideCosts{this, "hide-costs", "hide the costs", false};
   OptionalArgument<bool> hideLookups{this, "hide-lookups", "hide the lookups", false};
   OptionalArgument<bool> hideUpdates{this, "hide-updates", "hide the updates", false};
   OptionalArgument<bool> terse{this, "terse", "hide the unimportant metrics", false};

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
      infra::File data{args.vantageCSV.get(), File::AccessMode::ReadOnly};
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
      .requiredLookupOps = Rate::secondly(lookups),
      .lookupZipf = args.lookupZipf,
      .requiredUpdateOps = Rate::secondly(updates),
      .tupleSize = args.tupleSize,
      .pageSize = args.pageSize,
      .minSecondaries = static_cast<unsigned>(args.minReplicas.get()),
      .maxSecondaries = static_cast<unsigned>(args.maxReplicas.get()),
      .intraAZLatency = args.intraAZLatency,
      .interAZLatency = args.interAZLatency,
      .groupCommit = args.groupCommit,
      .deployAcrossAZ = args.deployAcrossAZ,
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
   if (!args.terse) registry.add<DatasetSize>(datasetSizeInBytes);
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
   if (!args.sortOrder.get().empty()) {
     registry.sort(args.sortOrder.get());
   }
   if (args.filter.get()) {
     registry.filter();
   }
   // Trunc only makes sense if values are sorted
   if (args.trunc.get()) {
     if (args.sortOrder.get().empty()) {
        cerr << "Specified trunc but no sorting, this doesn't make sense.\n";
        return 1;
     }
     registry.trunc(10);
   }
   registry.print(cout);
}
//--------------------------------------------------------------------------------
