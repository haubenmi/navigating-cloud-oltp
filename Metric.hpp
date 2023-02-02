#pragma once
//--------------------------------------------------------------------------------
#include "infra/Parser.hpp"
#include "infra/Terminal.hpp"
#include <compare>
#include <string>
#include <iostream>
#include <iomanip>
struct Architecture;
//--------------------------------------------------------------------------------
// A metric is like a visitor, but can be derived
struct Metric {
   static struct HiddenTag {
   } Hidden;
   virtual ~Metric() = default;
  //   MetricRegistry& metricRegistry;
   std::string name;
   uint64_t printWidth;
   bool hidden = false;

   enum class Align { Left,
                      Right };
   Align align = Align::Right;

   Metric(const std::string& name, uint64_t printWidth, Align align) : name{name}, printWidth{printWidth}, align{align} {}
   Metric(const std::string& name, uint64_t printWidth) : Metric{name, printWidth, Align::Right} {}
   Metric(const std::string& n) : Metric{n, n.size()} { }
   Metric(const std::string& n, HiddenTag) : Metric{n, n.size() + 2} { hidden = true; }

   void formatHeader(std::ostream& out);
   virtual void formatValue(std::ostream& out, const Architecture&, bool = false) { out << "----"; }
   virtual std::partial_ordering compare(const Architecture&, const Architecture&) const { throw std::runtime_error("sort not implemented for this visitor"); }
   virtual bool shouldExclude(const Architecture&) const { return false; }

   virtual const char* getColor(const Architecture&) const { return infra::Terminal::NOCOLOR; }

   static void formatByte(std::ostream& out, uint64_t value, bool raw);
   static void formatPercentage(std::ostream& out, double value, bool raw);
};
//--------------------------------------------------------------------------------
