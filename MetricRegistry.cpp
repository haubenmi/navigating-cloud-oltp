#include "MetricRegistry.hpp"
#include "Architecture.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
using namespace std;
//--------------------------------------------------------------------------------
void MetricRegistry::insert(const Architecture& a) {
   architectures[static_cast<uint8_t>(a.getType())].push_back(&a);
}
//--------------------------------------------------------------------------------
void MetricRegistry::printHeader(std::ostream& out) {
   for (auto i = 0u; i < metrics.size(); ++i) {
      auto& v = *metrics[i];
      if (v.hidden && !showHidden) continue;
      if (csvFormat) {
         v.formatHeader(out);
         if ((i + 1) != metrics.size()) out << csvDelimiter;
      } else {
         if (v.align == Metric::Align::Left) {
            out << left;
         } else {
            out << right;
         }
         out << setw(v.printWidth);
         stringstream tmp;
         v.formatHeader(tmp);
         auto tmp2 = tmp.str();
         string trunc = tmp2.size() <= v.printWidth ? tmp2 : tmp2.substr(0, v.printWidth - 2) + "..";
         out << trunc;
         out << infra::Terminal::DARKGREY << "|" << infra::Terminal::NOCOLOR;
      }
   }
   out << "\n";
}
//--------------------------------------------------------------------------------
void MetricRegistry::printArch(std::ostream& out, const Architecture& a, size_t) {
   for (auto i = 0u; i < metrics.size(); ++i) {
      auto& metric = *metrics[i];
      if (metric.hidden && !showHidden) continue;
      if (csvFormat) {
         metric.formatValue(out, a, true /*raw*/);
         if ((i + 1) != metrics.size()) out << csvDelimiter;
      } else {
         if (metric.align == Metric::Align::Left) {
            out << left;
         } else {
            out << right;
         }
         out << metric.getColor(a);
         out << setw(metric.printWidth);
         stringstream tmp;
         metric.formatValue(tmp, a);
         auto tmp2 = tmp.str();
         string trunc = tmp2.size() <= metric.printWidth ? tmp2 : tmp2.substr(0, metric.printWidth - 2) + "..";
         out << trunc;
         out << infra::Terminal::DARKGREY << "|" << infra::Terminal::NOCOLOR;
      }
   }
   out << "\n";
}
//--------------------------------------------------------------------------------
void MetricRegistry::print(ostream& out) {
   uint64_t i = 0;
   if (!overallSort.empty()) {
      for (auto& a : overallSort) {
         printArch(out, *a, i);
         ++i;
      }
   } else {
      for (auto& aType : architectures) {
         for (auto& a : aType) {
            printArch(out, *a, i);
            ++i;
         }
      }
   }
}
//--------------------------------------------------------------------------------
void MetricRegistry::sortAndTrunc(string_view col, size_t minPerArch) {
   auto sortCols = infra::Parser::split(col, ',');
   vector<pair<bool,Metric*>> sortColRefs;
   for (auto& s : sortCols) {
     bool reverse = s[0] == '-';
     auto cmp = s.substr(reverse);
     for (auto& m : metrics) {
        if (m->name == cmp) {
           sortColRefs.push_back(make_pair(reverse, m.get()));
        }
     }
   }
   if (sortColRefs.empty() || (sortColRefs.size() != sortCols.size())) {
      throw runtime_error("unknown sort column(s) '"s + string(col) + "'");
   }
   auto comp = [&](const auto& a, const auto& b) {
      for (auto s : sortColRefs) {
         auto reverse = s.first;
         auto res = s.second->compare(*a, *b);
         if (res == 0) continue;
         if (res < 0) return !reverse;
         if (res > 0) return reverse;
      }
      return false;
   };
   for (auto& at : architectures) {
     auto elemsToConsider = std::min(at.size(), minPerArch);
      std::partial_sort(at.begin(), at.begin() + elemsToConsider, at.end(), comp);
      at.resize(elemsToConsider);
      for (auto& a : at) {
         overallSort.push_back(a);
      }
      //   std::sort(architectures.begin(), architectures.end(), comp);
   }
   std::sort(overallSort.begin(), overallSort.end(), comp);
}
//--------------------------------------------------------------------------------
void MetricRegistry::filter() {
   for (auto& at : architectures) {
      std::erase_if(at, [&](auto a) { return std::any_of(metrics.begin(), metrics.end(), [&](auto& m) { return m->shouldExclude(*a); }); });
   }
}
//--------------------------------------------------------------------------------
