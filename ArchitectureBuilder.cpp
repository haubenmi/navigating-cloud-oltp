#include "ArchitectureBuilder.hpp"
#include "AuroraArchitecture.hpp"
#include "ClassicArchitecture.hpp"
#include "SocratesArchitecture.hpp"
#include "DynamicArchitecture.hpp"
#include "HADRArchitecture.hpp"
#include "InMemArchitecture.hpp"
#include "RemoteBlockDeviceArchitecture.hpp"
#include <algorithm>
#include <cassert>
#include <regex>
#include <unordered_set>
#include <vector>
//--------------------------------------------------------------------------------
using namespace std;
//--------------------------------------------------------------------------------
static InstanceStorage deriveInstanceStorage(const VantageSchema& instance) {
  uint64_t size = 1_gib * instance.storage_size.getUInt();
  double deviceCount = instance.storage_devices.getDouble(); // Can be fractions
  InstanceStorage::Type type = InstanceStorage::Type::None;
  uint64_t readOps = 0;
  uint64_t writeOps = 0;
  if (instance.storage_type.get() == "nvme") {
     type = InstanceStorage::Type::NVMe;
     readOps = instance.storage_readops.getUInt();
     writeOps = instance.storage_writeops.getUInt();
     assert(readOps > 0);
     assert(writeOps > 0);
  } else if (instance.storage_type.get() == "ssd") {
     type = InstanceStorage::Type::SSD;
     readOps = deviceCount * InstanceStorage::SSDReadOps;
     writeOps = deviceCount * InstanceStorage::SSDWriteOps;
  } else if (instance.storage_type.get() == "hdd") {
     type = InstanceStorage::Type::HDD;
     readOps = deviceCount * InstanceStorage::HDDReadOps;
     writeOps = deviceCount * InstanceStorage::HDDWriteOps;
  }
  return {type, size, deviceCount, readOps, writeOps};
}
//--------------------------------------------------------------------------------
static Network deriveNetworkSpeed(const VantageSchema& instance) {
   uint64_t speed = instance.network_speed.getDouble() * 1e9 /*Gbits*/;
   uint64_t burstSpeed = instance.network_speed_burst.getDouble() * 1e9 /*Gbits*/;
   auto upTo = instance.network_upto.get();
   if (upTo && (burstSpeed == speed)) {
      cerr << "WARN: '" << instance.name.get() << "' baseline speed and burst speed are the same for \n";
   }
   assert(burstSpeed >= speed);
   //  assert((burstSpeed > speed) == upTo);

   //   cout  << "spped = " << speed << "\n";
   //   cout << "dev: " << instance.network_devices.getUInt() << "\n";
   return Network{speed, burstSpeed, instance.network_devices.getUInt(), upTo};
}
//--------------------------------------------------------------------------------
static CPU deriveCPU(const VantageSchema& instance) {
  auto vcpu = instance.cpu.getUInt();
  auto speed = instance.clock.getDouble();
  auto vendor = instance.cpuVendor.get();
  if (speed == 0.0) speed = CPU::defaultSpeedGhz;
  speed *= 1e9;
  return CPU{vcpu, speed, vendor};
}
//--------------------------------------------------------------------------------
static MachineEBSLimits deriveMachineEBS(const VantageSchema& instance) {
  auto baseIops = instance.ebs_base_iops.getUInt();
  auto burstIops = instance.ebs_burst_iops.getUInt();
  assert(baseIops <= burstIops);

  auto baseThroughput = instance.ebs_base_throughput.getDouble();
  auto burstThroughput = instance.ebs_burst_throughput.getDouble();
  assert(baseThroughput <= burstThroughput);

  return MachineEBSLimits{.baseIops = Rate::secondly(baseIops),
                          .burstIops = Rate::secondly(burstIops),
                          .baseThroughput = baseThroughput * 1_mib,
                          .burstThroughput = burstThroughput * 1_mib};
}
//--------------------------------------------------------------------------------
// From https://stackoverflow.com/questions/30495102/iterate-through-different-subset-of-size-k
template<typename BidiIter, typename CBidiIter,
         typename Compare = std::less<typename BidiIter::value_type>>
int next_comb(BidiIter first, BidiIter last,
              CBidiIter /* first_value */, CBidiIter last_value,
              Compare comp=Compare()) {
  /* 1. Find the rightmost value which could be advanced, if any */
  auto p = last;
  while (p != first && !comp(*(p - 1), *--last_value)) --p;
  if (p == first) return false;
  /* 2. Find the smallest value which is greater than the selected value */
  for (--p; comp(*p, *(last_value - 1)); --last_value) { }
  /* 3. Overwrite the suffix of the subset with the lexicographically smallest
   *    sequence starting with the new value */
  while (p != last) *p++ = *last_value++;
  return true;
}
//--------------------------------------------------------------------------------
ArchitectureBuilder::ArchitectureBuilder(const VantageCSV& instances, Parameter p, std::string instanceFilterString, const std::vector<std::string>& architectures, const std::vector<std::string>& excludedArchitectures)
   : instanceTypes{instances}, p{p} {

   auto filters = infra::Parser::split(instanceFilterString, ',');
   if (filters.size() != 1 || filters[0] != "") {
      instanceFilter = std::move(filters);
   }

   assembleArchitectures(architectures,excludedArchitectures);
}
//--------------------------------------------------------------------------------
void ArchitectureBuilder::prepareNodes() {
   nodes.clear();
   for (auto& instanceType : instanceTypes) {
      auto name = regex_replace(regex_replace(instanceType.name.get(), regex("large"), "l"), regex("medium"), "m");
      name = regex_replace(name, regex("([0-9]+)xl$"), "$1");
      if (!instanceType.consider.get()) continue;
      if (instanceType.category.get() != "cpu") continue;
      //      if (instanceType.generation.get() != "current") continue; // TODO: I forgot to add generation to the csv file
      if (instanceType.name.get().find("metal") != string::npos) continue;
      auto mem = Memory(1024_mib * instanceType.memory.getDouble());
      auto cpu = deriveCPU(instanceType);
      auto price = Price::hourly(instanceType.price.getDouble() * (1.0 - p.ec2Discount));
      auto network = deriveNetworkSpeed(instanceType);
      auto iStorage = deriveInstanceStorage(instanceType);
      if (iStorage.type != InstanceStorage::Type::NVMe && iStorage.type != InstanceStorage::Type::None) continue;
      auto ebs = deriveMachineEBS(instanceType);
      nodes.push_back(Node{name, cpu, mem, network, price, iStorage, ebs});
   }
   // Put copies of each element in the vector
   cerr << "num instances: " << nodes.size() << "\n";
   // Build architectures out of `nodes` by iterating through all subsets
   std::sort(nodes.begin(), nodes.end(), [](const Node& a, const Node& b) {
      return a.name < b.name;
   });
}
//--------------------------------------------------------------------------------
bool ArchitectureBuilder::considerInstance(const Node& n) const {
  if (instanceFilter.empty()) return true;
  for (auto& i : instanceFilter) {
     if (n.name.starts_with(i)) return true;
  }
  return false;
}
//--------------------------------------------------------------------------------
void ArchitectureBuilder::assembleBasic() {
  uint64_t before = architectures.size();
  if (p.minSecondaries > 0) return;
  // The whole dataset has to fit on the single machine
  for (auto& n : nodes) {
     if (!considerInstance(n)) continue;
     auto arch = Classic::assemble(p, n);
     if (arch) {
        architectures.push_back(std::move(arch));
     }
  }
  cerr << "Create Classic architectures: " << (architectures.size() - before) << "\n";
}
//--------------------------------------------------------------------------------
void ArchitectureBuilder::assembleRemoteBlockDevice() {
  uint64_t before = architectures.size();
  if (p.minSecondaries > 0) return;
  // The whole dataset has to fit on the single machine
  for (auto& n : nodes) {
     if (!considerInstance(n)) continue;
     using T = EBS::Type;
     for (auto t : {T::gp3, T::gp2, T::io2, T::io1}) {
        auto arch = RemoteBlockDevice::assemble(p, n, t);
        if (arch) {
           architectures.push_back(std::move(arch));
        } else {
          //          cerr << "error with rbd: " << n.name << "\n";
        }
    }
  }
  cerr << "Create VBD architectures: " << (architectures.size() - before) << "\n";
}
//--------------------------------------------------------------------------------
void ArchitectureBuilder::assembleHadr() {
  uint64_t before = architectures.size();
  // The whole dataset has to fit on the single machine
  for (auto& n : nodes) {
     if (!considerInstance(n)) continue;
     for (unsigned i = p.minSecondaries; i <= p.maxSecondaries; ++i) {
        if (i == 0) continue; // HADR always has at least one secondary
        auto p2 = p;
        p2.numSecondaries = i;
        auto arch = HADR::assemble(p2, n);
        if (arch) {
           architectures.push_back(std::move(arch));
        }
     }
  }
  cerr << "Create HADR architectures: " << (architectures.size() - before) << "\n";
}
//--------------------------------------------------------------------------------
void ArchitectureBuilder::assembleInMem() {
  uint64_t before = architectures.size();
  if (p.minSecondaries > 0) return;
  for (auto& n : nodes) {
     if (!considerInstance(n)) continue;
     auto arch = InMemory::assemble(p, n);
     if (arch) {
        architectures.push_back(std::move(arch));
     }
  }
  cerr << "Create in-mem architectures: " << (architectures.size() - before) << "\n";
}
//--------------------------------------------------------------------------------
void ArchitectureBuilder::assembleAuroraLike() {
   uint64_t before = architectures.size();
   auto getStorageInstances = [&]() {
      unordered_map<string, Node> result;

      for (auto& n : nodes) {
         if (!n.instanceStorage) continue;
         auto type = n.getInstanceType();
         if (auto iter = result.find(type); iter != result.end()) {
            if (iter->second.cpu.count < n.cpu.count) {
               iter->second = n;
            }
         } else {
            result.emplace(make_pair(type, n));
         }
      }
      return result;
   };
   auto paretoInstances = [&]() {
      auto res = getStorageInstances();
      unordered_map<string, Node> result;

      for (auto& r : res) {
        bool keep = true;
        for (auto& d : res) {
          auto& n = r.second;
          auto& dn = d.second;
          if (dn.network.getReadLimit() > n.network.getReadLimit() && dn.instanceStorage.isParetoBetter(n.instanceStorage) && dn.price < n.price) {
            keep = false;
            break;
          }
        }
        if (keep) {
          result.insert(r);
        }
      }
      return result;
   };
   cerr << "Aurora storage nodes: (" << paretoInstances().size() << ")\n";
   for (auto& s : paretoInstances()) {
      for (auto& n : nodes) {
         if (!considerInstance(n)) continue;
         for (unsigned i = p.minSecondaries; i <= std::min(p.maxSecondaries, AuroraLike::maxSecondaries); ++i) {
            Parameter p2 = p;
            p2.numSecondaries = i;
            auto arch = AuroraLike::assemble(p2, n, s.second);
            if (arch) {
               if (arch->getDurability() >= p2.requiredDurability) {
                  architectures.push_back(std::move(arch));
               }
            }
         }
      }
   }
   cerr << "Create Aurora architectures: " << (architectures.size() - before) << "\n";
}
//--------------------------------------------------------------------------------
void ArchitectureBuilder::assembleSocrates() {
   uint64_t before = architectures.size();
   auto getInstances = [&]() {
      unordered_map<string, Node> result;

      for (auto& n : nodes) {
         if (!n.instanceStorage) continue;
         auto type = n.getInstanceType();
         // Only choose the largest instance per type
         if (auto iter = result.find(type); iter != result.end()) {
            if (iter->second.cpu.count < n.cpu.count) {
               iter->second = n;
            }
         } else {
            result.emplace(make_pair(type, n));
         }
      }
      return result;
   };

   auto paretoInstances = [&]() {
      auto res = getInstances();
      unordered_map<string, Node> pareto;

      for (auto& r : res) {
        bool keep = true;
        for (auto& d : res) {
          auto& n = r.second;
          auto& dn = d.second;
          if (dn.network.getReadLimit() > n.network.getReadLimit() && dn.instanceStorage.isParetoBetter(n.instanceStorage) && dn.price < n.price) {
            keep = false;
            break;
          }
        }
        if (keep) {
          pareto.insert(r);
        }
      }
      vector<Node> result;
      result.reserve(pareto.size());
      for (auto& r : pareto) {
        result.push_back(r.second);
      }
      return result;
   };
   auto logInstances = [&]() {
      if (p.requiredUpdateOps == Rate::zero) {
        // If there is nothing to log, just choose the cheapest instance
        Node winner = nodes.front();
        for (auto& n : nodes) {
          if (n.price < winner.price) winner = n;
        }

        return vector<Node>{winner};
      } else return paretoInstances();
   };
   cerr << "Considered page servers for Socrates (" << paretoInstances().size() << "): ";
   for (auto& pageNode : paretoInstances()) {
     cerr << pageNode.name << ",";
   }
   cerr << "\n";
   cerr << "Considered log servers for Socrates (" << logInstances().size() << "): ";
   // for (auto& pageNode : filterPageInstances()) {
   //   cerr << "pageNode: " << pageNode.second.name << " " << pageNode.second.price << "\n";
   // }
   // exit(0);

   if (p.requiredDurability <= SocratesLike::durability) {
      for (auto& pageNode : paretoInstances()) {
         for (auto& logNode : logInstances()) {
            for (auto& n : nodes) {
               if (!considerInstance(n)) continue;
               for (unsigned i = p.minSecondaries; i <= p.maxSecondaries; ++i) {
                  auto p2 = p;
                  p2.numSecondaries = i;
                  auto arch = SocratesLike::assemble(p2, n, pageNode, logNode);
                  if (arch) {
                     architectures.push_back(std::move(arch));
                  } else if (auto arch = SocratesLike::assemble(p2, n, pageNode, logNode, false)) {
                     // Try again without rbpex, to avoid strange effects
                     architectures.push_back(std::move(arch));
                  }
               }
            }
         }
      }
   }
   cerr << "Create Socrates architectures: " << (architectures.size() - before) << "\n";
}
//--------------------------------------------------------------------------------
void ArchitectureBuilder::assembleDynamic() {

   uint64_t before = architectures.size();
   auto getInstances = [&]() {
      unordered_map<string, Node> result;

      for (auto& n : nodes) {
         if (!n.instanceStorage) continue;
         auto type = n.getInstanceType();
         // Only choose the largest instance per type
         if (auto iter = result.find(type); iter != result.end()) {
            if (iter->second.cpu.count < n.cpu.count) {
               iter->second = n;
            }
         } else {
            result.emplace(make_pair(type, n));
         }
      }
      return result;
   };

   auto paretoInstances = [&]() {
      auto res = getInstances();
      unordered_map<string, Node> pareto;

      for (auto& r : res) {
        bool keep = true;
        for (auto& d : res) {
          auto& n = r.second;
          auto& dn = d.second;
          if (dn.network.getReadLimit() > n.network.getReadLimit() && dn.instanceStorage.isParetoBetter(n.instanceStorage) && dn.price < n.price) {
            keep = false;
            break;
          }
        }
        if (keep) {
          pareto.insert(r);
        }
      }
      vector<Node> result;
      result.reserve(pareto.size());
      for (auto& r : pareto) {
        result.push_back(r.second);
      }
      return result;
   };
   auto logInstances = [&]() {
      if (p.requiredUpdateOps == Rate::zero) {
        // If there is nothing to log, just choose the cheapest instance
        Node winner = nodes.front();
        for (auto& n : nodes) {
          if (n.price < winner.price) winner = n;
        }

        return vector<Node>{winner};
      } else return paretoInstances();
   };

   for (auto& n : nodes) {
      if (!considerInstance(n)) continue;
      auto arches = Dynamic::assemble(p, n, paretoInstances(), logInstances());
      for (auto& a : arches) {
         architectures.push_back(std::move(a));
      }
   }

   cerr << "Create Dynamic architectures: " << (architectures.size() - before) << "\n";
}
//--------------------------------------------------------------------------------
void ArchitectureBuilder::assembleArchitectures(const vector<string>& architectures, const vector<string>& excludedArchitectures) {
   prepareNodes();

   // std::sort(nodes.begin(),nodes.end(), [](auto& a, auto& b) { return a.getPricePerGBMemory() < b.getPricePerGBMemory(); });
   // for (auto& n : nodes) {
   //   cout << n.name << " -> " << n.getPricePerGBMemory() << " /GB DRAM\n";
   // }
   // exit(1);

   for (auto& a : architectures) {
      cerr << "Building arch: " << a << "\n";
  }
  unordered_set<string> archs{architectures.begin(),architectures.end()};
  unordered_set<string> excludes{excludedArchitectures.begin(),excludedArchitectures.end()};
  if ((archs.empty() || archs.contains(archTypeToName(ArchType::Classic))) && !excludes.contains(archTypeToName(ArchType::Classic))) {
     assembleBasic();
  }
  if ((archs.empty() || archs.contains(archTypeToName(ArchType::RemoteBlockDevice))) && !excludes.contains(archTypeToName(ArchType::RemoteBlockDevice))) {
     assembleRemoteBlockDevice();
  }
  if ((archs.empty() || archs.contains(archTypeToName(ArchType::HADR))) && !excludes.contains(archTypeToName(ArchType::HADR))) {
     assembleHadr();
  }
  if ((archs.empty() || archs.contains(archTypeToName(ArchType::InMemory))) && !excludes.contains(archTypeToName(ArchType::InMemory))) {
     assembleInMem();
  }
  if ((archs.empty() || archs.contains(archTypeToName(ArchType::AuroraLike))) && !excludes.contains(archTypeToName(ArchType::AuroraLike))) {
     assembleAuroraLike();
  }
  if ((archs.empty() || archs.contains(archTypeToName(ArchType::SocratesLike))) && !excludes.contains(archTypeToName(ArchType::SocratesLike))) {
     assembleSocrates();
  }
  if ((archs.empty() || archs.contains(archTypeToName(ArchType::Dynamic))) && !excludes.contains(archTypeToName(ArchType::Dynamic))) {
     assembleDynamic();
  }

  cerr << "Num assembled architectures: " << architectures.size() << "\n";
}
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
