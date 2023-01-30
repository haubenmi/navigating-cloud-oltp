#pragma once
#include "Common.hpp"
#include "Architecture.hpp"
#include <iomanip>
//--------------------------------------------------------------------------------
using namespace std::chrono;
//--------------------------------------------------------------------------------
struct ArchitectureBuilder {

   const VantageCSV& instanceTypes;
   Parameter p;
   std::vector<std::string> instanceFilter;
   std::vector<Node> nodes;
   std::vector<std::unique_ptr<Architecture>> architectures;

   ArchitectureBuilder(const VantageCSV& instances, Parameter p, std::string instanceFilter, const std::vector<std::string>& architectures, const std::vector<std::string>& excludedArchitectures);

   void assembleArchitectures(const std::vector<std::string>& architectures, const std::vector<std::string>& excludedArchitectures);
   auto& getArchitectures() { return architectures; }

   private:
   void assembleBasic();
   void assembleRemoteBlockDevice();
   void assembleHadr();
   void assembleInMem();
   void assembleAuroraLike();
   void assembleSocrates();
   void assembleDynamic();

   void prepareNodes();

   bool considerInstance(const Node& n) const;
};
