#pragma once
#include "Metric.hpp"
#include <array>
#include <iostream>
#include <memory>
#include <vector>
//--------------------------------------------------------------------------------
struct Architecture;
//--------------------------------------------------------------------------------
struct MetricRegistry {
   std::vector<std::unique_ptr<Metric>> metrics;
   std::array<std::vector<const Architecture*>, 7> architectures;
   std::vector<const Architecture*> overallSort;
   //  std::vector<std::vector<std::unique_ptr<Metric>>> results;
   bool csvFormat;
   bool showHidden;
   bool hideAddedMetrics = false;
   std::string csvDelimiter;

   MetricRegistry(bool csvFormat = false, bool showHidden = false, std::string csvDelimiter = ",") : csvFormat{csvFormat}, showHidden{showHidden}, csvDelimiter{csvDelimiter} {}
   void sortAndTrunc(std::string_view sortColumn, size_t minPerArch);
  //   void trunc(size_t minPerArch);
   void filter();

   template <typename T, typename... Args>
   void add(Args&&... args);

   void printHeader(std::ostream& out);
   void insert(const Architecture& a);
   void printArch(std::ostream& out, const Architecture& a, size_t id);
   void print(std::ostream& out);

   void hideNextMetrics(bool hide) { hideAddedMetrics = hide; }
};
//--------------------------------------------------------------------------------
template <typename T, typename... Args>
void MetricRegistry::add(Args&&... args) {
   metrics.push_back(std::make_unique<T>(std::forward<Args>(args)...));
   if (hideAddedMetrics) {
      this->metrics.back()->hidden = true;
   }
}
//--------------------------------------------------------------------------------
