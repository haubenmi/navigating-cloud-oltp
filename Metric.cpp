#include "Metric.hpp"
#include <iomanip>
#include <sstream>
//--------------------------------------------------------------------------------
using namespace std;
//--------------------------------------------------------------------------------
void Metric::formatByte(std::ostream& out, uint64_t value, bool raw) {
   if (raw) {
      out << value;
   } else {
      stringstream ss;
      infra::BinaryUnitInterpreter::print(ss, value);
      ss << "b";
      out << ss.str();
   }
}
//--------------------------------------------------------------------------------
void Metric::formatPercentage(std::ostream& out, double value, bool raw) {
   if (raw) {
      out << value;
   } else {
      stringstream ss;
      ss << fixed << setprecision(1) << 100 * value << "%";
      out << ss.str();
   }
}
//--------------------------------------------------------------------------------
void Metric::formatHeader(ostream& out) {
  out << name;
}
//--------------------------------------------------------------------------------
