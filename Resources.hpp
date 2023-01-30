#pragma once
//--------------------------------------------------------------------------------
#include <cstdint>
#include <cmath>
#include <compare>
#include <iosfwd>
#include "Common.hpp"
//--------------------------------------------------------------------------------
struct FailureMode;
struct Node;
//--------------------------------------------------------------------------------
struct Latency {
   static bool machineReadable;
   static constexpr bool verbose = false;
   nanoseconds min, avg, max;
   constexpr Latency() : min{1000h}, avg{1000h}, max{1000h} {}
   static Latency infinite() { return Latency{1000h, 1000h, 1000h}; }
   constexpr explicit Latency(nanoseconds x) : min{x}, avg{x}, max{x} {}
   constexpr Latency(nanoseconds min, nanoseconds avg, nanoseconds max) : min{min}, avg{avg}, max{max} {}
   nanoseconds get() const { return avg; }
   Latency fix() const;
   Latency asAvg() const { return Latency{avg}; }
   static Latency combine(std::initializer_list<std::pair<double, Latency>> weights);
   static Latency deduce(Latency target, std::initializer_list<std::pair<double, Latency>> weights);
   // Give the ratio of lower [0.0-1.0] needed to reach target latency
   static double getRatio(Latency target, Latency lower, Latency higher);
   Latency operator+(const Latency& other) const { return Latency{min + other.min, avg + other.avg, max + other.max}.fix(); }
   Latency operator-(const Latency& other) const { return Latency{(min > other.min) ? (min - other.min) : 0ns, (avg > other.avg) ? (avg - other.avg) : 0ns, (max > other.max) ? (max - other.max) : 0ns}.fix(); }
};
std::ostream& operator<<(std::ostream&, const Latency& p);
//--------------------------------------------------------------------------------
struct Location {
   virtual Latency getLatency() = 0;
   virtual ~Location() = default;
};
struct SameInstance : public Location {
   Latency getLatency() override { return Latency{1us}; }
};
struct SameDatacenter : public Location { // Same DC, other instance
   static constexpr Latency latency{78us, 90us, 116us};
   // https://blog-en.richardimaoka.net/network-latency-analysis-with-ping-aws
   Latency getLatency() override { return latency; }
};
struct SameRegion : public Location { // Same Region, different AZ
   static constexpr Latency latency{1500us, 2000us, 2400us};
   // https://docs.aws.amazon.com/sap/latest/general/arch-guide-architecture-guidelines-and-decisions.html
   //  https://stackoverflow.com/questions/54190445/aws-latency-between-zones-within-a-same-region
   Latency getLatency() override { return latency; }
};
struct OtherRegion : public Location { // Different Region
   // https://www.cloudping.co/grid
   // Taking as an example here eu-central-1 -> us-east-1
  Latency getLatency() override { return Latency{89ms,90ms,93ms}; }
};
//--------------------------------------------------------------------------------
enum class Timeunit { Second,
                      Minute,
                      Hour,
                      Day,
                      Month,
                      Year };
//--------------------------------------------------------------------------------
struct Price {
   double value = 0;
   static Timeunit timeunitForPrint;
   static bool machineReadable;
   enum class Bill { PerHour,
                     PerRequest } bill;
   enum class Category { OnDemand,
                         Spot } category = Category::OnDemand;

   static constexpr Price secondly(double val = 0) { return Price{val * 3600, Bill::PerHour}; }
   static constexpr Price minutely(double val = 0) { return Price{val * 60, Bill::PerHour}; }
   static constexpr Price hourly(double val = 0) { return Price{val, Bill::PerHour}; }
   static constexpr Price daily(double val = 0) { return Price{val / 24, Bill::PerHour}; }
   static constexpr Price monthly(double val = 0) { return Price{val / 30 / 24, Bill::PerHour}; }

   static constexpr Price perThousand(double val = 0) { return Price{val / 1000, Bill::PerRequest}; }

   static const Price zero;

   void operator+=(Price other) {
      assert(bill == other.bill);
      value += other.value;
   }
   Price operator+(Price other) const {
     assert(bill == other.bill);
     other.value += value;
     return other;
   }
   bool operator<(const Price& other) const {
      assert(bill == other.bill);
      return value < other.value;
   }
   auto operator<=>(const Price& other) const = default;
   protected:
   constexpr Price(double v, Bill b) : value{v}, bill{b} {}
};
constexpr Price Price::zero = Price{0, Price::Bill::PerHour};
std::ostream& operator<<(std::ostream&, const Price& p);
//--------------------------------------------------------------------------------
struct Rate {
  // Normalized to secondly
  double rate;

  static constexpr Rate secondly(double v) { return Rate{v}; }
  static constexpr Rate hourly(double v) { return Rate{v/3600}; }

  static const Rate unlimited;
  static const Rate zero;

  bool operator==(const Rate& other) const = default;
  auto operator<=>(const Rate& other) const = default;
  auto operator*(double m) const { return Rate{rate * m}; }
  auto operator/(Rate m) const { return rate / m.rate; }
  auto operator/(double m) const { return (m==0.0) ? unlimited : Rate{rate / m}; }
  auto operator+(Rate o) const { return Rate{rate + o.rate}; }
  Rate& operator+=(Rate o) { rate += o.rate; return *this; }
  Rate& operator-=(Rate o) {
     if (*this != unlimited) rate -= o.rate;
     return *this;
  }
  auto operator-(Rate o) const {
    if (o.rate > rate) {
      auto res = o.rate - rate;
      if (res < 0.000001) return Rate::zero;
    }
    assert(rate >= o.rate);
    return Rate{rate - o.rate};
  }

  double duration() const { return 1 / rate; }

  uint64_t nextInt() const { return std::ceil(rate); }

  Rate roundDown() const { return Rate(std::floor(rate)); }
  Rate roundUp() const { return Rate(std::ceil(rate)); }

  protected:
  constexpr explicit Rate(double v) : rate{v} {}
};
constexpr Rate Rate::zero = Rate(0);
constexpr Rate Rate::unlimited = Rate(99999999999);
std::ostream& operator<<(std::ostream&, const Rate& r);
//--------------------------------------------------------------------------------
Price operator*(Price, Rate);
Price operator*(double mul, Price p);
//--------------------------------------------------------------------------------
struct Nines {
};
static constexpr Nines nines;
//--------------------------------------------------------------------------------
struct Durability {
   enum class FailureScope { Instance,
                             AZ,
                             Region };
   enum class FailurePersistence { Transient,
                                   Permanent };
   static bool machineReadable;
   double numericValue;

   constexpr Durability(double n) : numericValue{std::nextafter(n, 1.0)} {
      assert(numericValue <= 1.0);
   }

  //   constexpr Durability(double n);
   constexpr Durability(unsigned n, Nines) {
      double result = 0;
      double delta = 0.9;
      for (unsigned i = 0; i < n; ++i) {
         result += delta;
         delta *= 0.1;
      }
      //      result += 0.1 * delta; // Add another delta, but skip one order of magnitude, to avoid rounding issues
      numericValue = result;
      assert(numericValue <= 1.0);
   }
   auto operator<=>(const Durability& other) const = default;
   static constexpr Durability calculateDurability(unsigned numNodes, double nodeAvailabilityPerMonth, uint64_t mttr, unsigned minNodesForDurability = 1) {
      // What is the probability that inside one MTTR (copy whole dataset to a new instance) all instances fail?
      auto afr = 1.0 - nodeAvailabilityPerMonth;
      uint64_t secondsInYear = 3600 * 24 * 365;
      uint64_t secondsInMonth = 3600 * 24 * 30;
      // Lambda is the avg. number of failures we expect
      double lambda = (numNodes * afr * mttr) / secondsInMonth;
      // Our probability for being durable in one MTTR interval is if we loose less than n instances.
      double result = 0.0;
      for (unsigned i = 0; i <= numNodes - minNodesForDurability; ++i) {
         double fact = tgamma(i + 1);
         double v = (exp(-lambda) * pow(lambda, i)) / fact;
         result += v;
      }
      if (result > 1.0) result = 1.0; // Fix rounding issues

      // Our probability for being durable over a whole year is the probability that we are durable in *all* intervals.
      auto intervalsPerYear = secondsInYear / mttr;
      auto d = std::pow(result, intervalsPerYear);
      assert(d <= 1.0000);
      return Durability(d);
   }
};
std::ostream& operator<<(std::ostream&, const Durability& r);
//--------------------------------------------------------------------------------
struct FailoverTime {
   static bool machineReadable;
   double value;

   constexpr FailoverTime(double seconds) : value{seconds} {}

   auto operator<=>(const FailoverTime& other) const { return other.value <=> value; }

   FailoverTime operator+(FailoverTime other) const { return FailoverTime{value + other.value}; }
};
std::ostream& operator<<(std::ostream&, const FailoverTime& f);
//--------------------------------------------------------------------------------
struct CPU {
   static constexpr double defaultSpeedGhz = 2.2;
  //   static constexpr uint64_t cyclesPerWrite = 4000;
  //   static constexpr uint64_t cyclesPerRead = 4000;
   uint64_t count;
   double speed; // in hz
   std::string vendor;

   bool operator==(const CPU& other) const = default;
   std::partial_ordering operator<=>(const CPU& other) const {
      if (auto cmp = count <=> other.count; cmp != 0) return cmp;
      return speed <=> other.speed;
      // exclude vendor
   }


   Rate getWriteOps(uint64_t cyclesPerUpdate) const { return Rate::secondly((count * speed) / cyclesPerUpdate); }
   Rate getReadOps(uint64_t cyclesPerLookup) const { return Rate::secondly((count * speed) / cyclesPerLookup); }
   Rate getOps(uint64_t cyclesPerOp) const { return Rate::secondly((count * speed) / cyclesPerOp); }
};
//--------------------------------------------------------------------------------
struct Memory {
   static constexpr Latency readLatency{555ns}; // Corresponds to 4000 cycles at 2Ghz

   uint64_t size;
   Memory(uint64_t bytes) : size{bytes} {}
   Memory operator*(uint64_t num) const { return Memory{size * num}; }
   Memory operator+(Memory other) const { return Memory{size + other.size}; }
   void operator+=(Memory other) { size += other.size; }
   bool operator==(const Memory& other) const = default;
   auto operator<=>(const Memory& other) const = default;
   static Memory kb(uint64_t c) { return Memory{1024 * c}; }
   static Memory mb(uint64_t c) { return kb(1024 * c); }
   static Memory gb(uint64_t c) { return mb(1024 * c); }

   uint64_t getTotalSize() const { return size; }
};
//--------------------------------------------------------------------------------
struct InstanceStorage {
   static constexpr auto SSDReadOps = 100'000;
   static constexpr auto SSDWriteOps = 50'000;
   static constexpr auto HDDReadOps = 100;
   static constexpr auto HDDWriteOps = 100;
   static constexpr auto MaxIOPSize = 4_kib;

   enum class Type { NVMe,
                     SSD,
                     HDD,
                     None } type;

   /// Size is per device
   uint64_t size;
   double devices; // How many physical disks
   uint64_t readOps;
   uint64_t writeOps;

   // Based on measurements on an i3en.24
   static constexpr auto NVMeReadPenalty = 0.8;
   static constexpr Latency writeLatency{44us};
   static constexpr Latency readLatency{132us};

   uint64_t getTotalSize() const { return devices * size; }
   Rate getReadOps() const { return Rate::secondly((type == Type::NVMe ? NVMeReadPenalty : 1.0) * readOps); }
   Rate getWriteOps() const { return Rate::secondly(writeOps); }
   uint64_t getWriteThroughput() const { return writeOps * MaxIOPSize; }
   uint64_t getReadThroughput() const { return readOps * MaxIOPSize; }

   bool operator==(const InstanceStorage& other) const = default;
   auto operator<=>(const InstanceStorage& other) const = default;
   // Leave 10% free on NVMe and SSD
   uint64_t getUsableSize() const { return getTotalSize() * ((type == Type::NVMe || type == Type::SSD) ? 0.9 : 1.0); }

   std::string storageTypeToString() const;
   std::string getDescription() const;
   operator bool() const { return devices != 0.0; }

   bool isParetoBetter(const InstanceStorage& other) const {
     return getUsableSize() > other.getUsableSize() && getReadOps() > other.getReadOps() && getWriteOps() > other.getWriteOps();
   }
};
//--------------------------------------------------------------------------------
struct InstanceStorageAllotment {
   uint64_t size = 0;
   Rate reads = Rate::zero;
   Rate writes = Rate::zero;
};
//--------------------------------------------------------------------------------
struct MachineEBSLimits {
  Rate baseIops;
  Rate burstIops;

  double baseThroughput;
  double burstThroughput;

  auto operator<=>(const MachineEBSLimits&) const = default;
};
//--------------------------------------------------------------------------------
struct EBS {
   enum class Type { gp3, // max. throughput 1000 MiB/s, durability 99.8%, capacity 16TiB, 16k IOPS
                     gp2, // max. throughput 250MiB/s, durability 99.8%, capacity 16Tib, 16k IOPS
                     // IOPS: min 100; max 3000, starting from 34gb you get 3 IOPS per gb up to 3000
                     io1, // max. throughput 1000MiB/s, durability 99.8%, capacity 16TiB, 64k IOPS
                     io2, // max. throughput 1000 MiB/s, durability 99.999%, capacity 16TiB, 64k IOPS
                     io2x }; // max. throughput 4000 MiB/s, durability 99.999%, capacity 64TiB, 256k IOPS, only on R5b instances

   /// See https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/ebs-volume-types.html#solid-state-drives
   static constexpr uint64_t sectorSize = 512;
   static constexpr uint64_t maxIopSize = 256_kib;

  struct Constraint {
    uint64_t minIops, maxIops;
    uint64_t minCapacity, maxCapacity;
    uint64_t minThroughput, maxThroughput;
    uint64_t maxIopsPerGB;
  };

  static constexpr Constraint gp3 = {.minIops = 0,
                                     .maxIops = 16000,
                                     .minCapacity = 1_gib,
                                     .maxCapacity = 16_tib,
                                     .minThroughput = 0,
                                     .maxThroughput = 1_gib,
                                     .maxIopsPerGB = 500};

  static constexpr Constraint gp2 = {.minIops = 100,
                                     .maxIops = 16000,
                                     .minCapacity = 1_gib,
                                     .maxCapacity = 16_tib,
                                     .minThroughput = 0,
                                     .maxThroughput = 250_mib,
                                     .maxIopsPerGB = 3};

  static constexpr Constraint io2 = {.minIops = 100,
                                     .maxIops = 64000,
                                     .minCapacity = 4_gib,
                                     .maxCapacity = 16_tib,
                                     .minThroughput = 0,
                                     .maxThroughput = 1_gib,
                                     .maxIopsPerGB = 500};

  static constexpr Constraint io2x = {.minIops = 100,
                                      .maxIops = 256000,
                                      .minCapacity = 4_gib,
                                      .maxCapacity = 64_tib,
                                      .minThroughput = 0,
                                      .maxThroughput = 4_gib,
                                      .maxIopsPerGB = 1000};

  static constexpr Constraint io1 = {.minIops = 100,
                                     .maxIops = 64000,
                                     .minCapacity = 4_gib,
                                     .maxCapacity = 16_tib,
                                     .minThroughput = 0,
                                     .maxThroughput = 1_gib,
                                     .maxIopsPerGB = 50};

  static constexpr double gp3_durability = 0.999;
  static constexpr double gp2_durability = 0.999;
  static constexpr double io2x_durability = 0.99999;
  static constexpr double io2_durability = 0.99999;
  static constexpr double io1_durability = 0.999;

  static constexpr Price gp3_storagePerGB = Price::monthly(0.08);
  static constexpr uint64_t gp3_free_iops = 3000;
  static constexpr Price gp3_iop = Price::monthly(0.005);
  static constexpr uint64_t gp3_free_throughput = 125_mib;
  static constexpr Price gp3_throughput = Price::monthly(0.04);

  static constexpr Price gp2_storagePerGB = Price::monthly(0.10);

  /// See https://aws.amazon.com/ebs/pricing/
  static constexpr Price io_storagePerGB = Price::monthly(0.125);
  static constexpr Price io_iop = Price::monthly(0.065);
  static constexpr Price io2_iopsAfter32k = Price::monthly(0.046);
  static constexpr Price iox_iopsAfter64k = Price::monthly(0.032);

  static constexpr Latency writeLatency{292us};
  static constexpr Latency readLatency{374us};


  /// Per device
  uint64_t size;
  uint64_t iops;
  uint64_t throughput;

  Type type;
  uint64_t numDevices;
  Price getSingleVolumePrice() const;

  EBS(uint64_t size, uint64_t iops, uint64_t throughput, Type type, uint64_t numDevices) : size{size}, iops{iops}, throughput{throughput}, type{type}, numDevices{numDevices} {}

  public:
  Price getPrice() const;
  bool operator==(const EBS& other) const = default;
  auto operator<=>(const EBS& other) const = default;

  static EBS createVolume(std::string_view instanceName, Type type, uint64_t capacity, uint64_t iops, uint64_t throughput, uint64_t iopSize);

  Rate getIOPS() const { return Rate::secondly(iops) * numDevices; }
  uint64_t getThroughput() const { return throughput * numDevices; }

  uint64_t getTotalSize() const { return size * numDevices; }
  uint64_t getNumDevices() const { return numDevices; }
  Type getType() const { return type; }
  static std::string getTypeName(Type t);

  Durability getDurability() const { return getDurability(type); }
  static Durability getDurability(Type);

  std::string getDescription() const;
};
//--------------------------------------------------------------------------------
struct EBSAllotment {
   EBS::Type type;
   uint64_t size = 0;
   Rate iops = Rate::zero;
   uint64_t bandwidth = 0;
   uint64_t maxIopSize = 0;
   std::string describe() const;
};
//--------------------------------------------------------------------------------
/// Unlimited capacity, IOPS depend on instance network, latency ~ 30ms
struct S3 {
  // Prices from https://aws.amazon.com/s3/pricing/ for us-west
  static constexpr Price first50TBPerGB = Price::monthly(0.023);
  static constexpr Price next450TBPerGB = Price::monthly(0.022);
  static constexpr Price over500TBPerGB = Price::monthly(0.021);

  static constexpr Price putPrice = Price::perThousand(0.005);
  static constexpr Price getPrice = Price::perThousand(0.0004);

  static constexpr Durability durability = Durability{11, nines};
  bool operator==(const S3& other) const = default;
  auto operator<=>(const S3& other) const = default;

  static uint64_t getTotalSize() { return std::numeric_limits<uint64_t>::max(); }
};
//--------------------------------------------------------------------------------
struct Network {

   // A c5.large can do sth. like 800k pps
   static constexpr uint64_t S3WriteTransferSize = 2_mib;
   static constexpr uint64_t S3ReadTransferSize = 2_mib;

   /// 1Cent per GB, but both for sender and receiver
   static constexpr Price interAZCost = Price::secondly(0.02);
   /// Speed in Gigabit/s
   uint64_t speed;
   /// Burst speed in Gigabit/s
   uint64_t burstSpeed;
   /// The number of network cards
   uint64_t devices = 1;
   // Depends on where other endpoint is
   //  uint64_t latency;
   // Is this not guaranteed?
   bool upTo;
   // Is this a vague speed?
   bool vague = false;

   bool operator==(const Network& other) const = default;
   auto operator<=>(const Network& other) const = default;
   size_t hash() const { return multihash(speed, burstSpeed, devices, upTo, vague); }

   Rate getS3WriteOps() const { return getWriteLimit() / S3WriteTransferSize; }
   Rate getS3ReadOps() const { return getReadLimit() / S3ReadTransferSize; }

   // In bytes
   Rate getReadLimit() const { return Rate::secondly(devices * speed / 8); }
   Rate getWriteLimit() const { return Rate::secondly(devices * speed / 8); }
};
//--------------------------------------------------------------------------------
struct Node {
   std::string name;
   CPU cpu;
   Memory memory;
   Network network;
   Price price;
   InstanceStorage instanceStorage;
   MachineEBSLimits machineEbs;

   // https://freeman.vc/notes/aws-vs-gcp-reliability-is-wildly-different
  // assume defensively one minute to boot up the OS and the DBMS
  //   static constexpr FailoverTime nodeSpinupTime{11.4};
   static constexpr FailoverTime nodeSpinupTime{60};
   static constexpr FailoverTime secondaryTakeover{5};
   bool operator==(const Node& other) const = default;

  Price getPricePerGBMemory() const { return Price::hourly(price.value / (memory.getTotalSize() / 1024 / 1024 / 1024 )); }

   Node(std::string name, CPU cpu, Memory mem, Network network, Price price, InstanceStorage iStorage, MachineEBSLimits machineEbs) : name{name}, cpu{cpu}, memory{mem}, network{network}, price{price}, instanceStorage{iStorage}, machineEbs{machineEbs} {}

   Node(const Node& n) = default;
   Node& operator=(const Node& n) = default;

   uint64_t maxEBSDevices() const {
      auto base = name.find("metal") ? 31 : 28;
      return base - network.devices - instanceStorage.devices;
   }
   std::partial_ordering operator<=>(const Node& other) const {
      if (auto cmp = cpu <=> other.cpu; cmp != 0) return cmp;
      if (auto cmp = memory <=> other.memory; cmp != 0) return cmp;
      if (auto cmp = network <=> other.network; cmp != 0) return cmp;
      if (auto cmp = price <=> other.price; cmp != 0) return cmp;
      if (auto cmp = instanceStorage <=> other.instanceStorage; cmp != 0) return cmp;
      return machineEbs <=> other.machineEbs;
   }
   Durability getAvailability() const { return Durability{0.995}; }

   Price getPrice() const { return price; }

   std::string getInstanceType() const { return infra::Parser::split(name, '.')[0]; }
};
//--------------------------------------------------------------------------------
struct Parameter {
   uint64_t datasetSize;
   double dataBloat;
   double usableMemory;
   double networkOverhead;
   Rate requiredLookupOps;
   double lookupZipf; /// For a 100GB dataset and 10GB buffer, we normally have 10% cache hits. With Zipf skew,
   Rate requiredUpdateOps;
   uint64_t tupleSize;
   uint64_t pageSize;
   uint64_t cpuCost;
   unsigned numSecondaries = 0;
   unsigned minSecondaries;
   unsigned maxSecondaries;
   double intraAZLatency;
   double interAZLatency;
   double ec2Discount;
   unsigned numberOfAZs = 3;

   std::string logServiceInstance = "i3en.24xl"; // Difference is that we don't need that much capacity
   uint64_t logServiceCapacityInSeconds = 3600;
  //   uint64_t logDeviceCapacityInSeconds = 100;
   uint64_t logServiceReplication = 6;
   uint64_t logRecordHeaderSize = 6 * 8;

   std::string pageServiceInstance = "i3en.24xl";
   unsigned pageServerReplication = 2;
   bool groupCommit = true;
   bool deployAcrossAZ = false;
   bool walIncludesUndo = false;
   /// The default are index-only tables, where all the data is stored in a single clustered b-tree
   /// The alternative means the keys are stored in an index together with PIDs, while the keys+values are stored again in a table heap
   /// A lookup requires finding the page in the index, and then do one more page load to get to the value
   /// An update requires to find the page in the index, and then load and update one additional page
   /// The total data size grows in this scenario
   bool indexOnlyTables = true;

   Latency requiredOpLatency;
   Durability requiredDurability;

   Rate requiredOps() const { return requiredLookupOps + requiredUpdateOps; }
   uint64_t numTuples() const { return datasetSize / tupleSize; }
   uint64_t getDataSize() const { return datasetSize * dataBloat; }
   // We assume an index entry to take 20 bytes
   uint64_t indexSize() const { return indexOnlyTables ? 0 : (numTuples() * 20); }
   // When we have secondaries. The primary can also take lookups (+1), but the first secondary is a standby node (-1), which cancels out
   Rate requiredOpsPerNode() const { return requiredUpdateOps + ((numSecondaries > 1) ? (requiredLookupOps / (numSecondaries + 1.0 - 1.0)) : requiredLookupOps); }
   uint64_t getRedoLogRecordSize() const { return tupleSize + logRecordHeaderSize; }
   uint64_t getAriesLogRecordSize() const { return 2 * tupleSize + logRecordHeaderSize; }
   uint64_t getLogRecordSize() const { return walIncludesUndo ? getAriesLogRecordSize() : getRedoLogRecordSize(); }
   uint64_t getRequiredLogStorageImpl(uint64_t logRecordSize) const { return requiredUpdateOps.rate * logServiceCapacityInSeconds * logRecordSize; }
   uint64_t getRequiredRedoLogStorage() const { return getRequiredLogStorageImpl(getRedoLogRecordSize()); }
   uint64_t getRequiredAriesLogStorage() const { return getRequiredLogStorageImpl(getAriesLogRecordSize()); }
   uint64_t getRequiredLogStorage() const { return getRequiredLogStorageImpl(getLogRecordSize()); }

   Rate getLogWritesRequiredForUpdates(uint64_t maxIopSize) const;

   double getRemoteAZRatio() const { return deployAcrossAZ ? ((numberOfAZs - 1.0) / numberOfAZs) : 0.0; }
   double getSameAZRatio() const { return deployAcrossAZ ? (1.0 / numberOfAZs) : 1.0; }
};
//--------------------------------------------------------------------------------
