#include "CSV.hpp"
#include <algorithm>
//--------------------------------------------------------------------------------
using namespace std;
//--------------------------------------------------------------------------------
namespace infra {
//--------------------------------------------------------------------------------
CSVValueBase::CSVValueBase(CSVSchema* schema, string_view name) : name{name} {
   schema->base->registerField(this, name);
}
//--------------------------------------------------------------------------------
void CSVNumber::parse(CSVReader& reader) {
   if (!reader.eol() && (reader.peek() != ",")) {
      auto token = reader.nextToken();
      if (auto d = Parser::tryParseDouble(token)) {
         value = *d;
      }
   }
}
//--------------------------------------------------------------------------------
bool CSVNumber::isInt() const {
  int64_t i = value;
  return static_cast<double>(i) == value;
}
//--------------------------------------------------------------------------------
int64_t CSVNumber::getInt() const {
   int64_t result = value;
   if (static_cast<double>(result) == value) {
      return result;
   } else {
      throw domain_error("csv number '" + to_string(value) + "' cannot be represent as an integer");
   }
}
//--------------------------------------------------------------------------------
uint64_t CSVNumber::getUInt() const {
   uint64_t result = value;
   if (static_cast<double>(result) == value) {
      return result;
   } else {
      throw domain_error("csv number '" + to_string(value) + "' cannot be represent as an unsigned integer");
   }
}
//--------------------------------------------------------------------------------
void CSVString::parse(CSVReader& reader) {
   while (!reader.eol() && (reader.peek() != ",")) {
      value += reader.nextToken();
   }
}
//--------------------------------------------------------------------------------
void CSVBool::parse(CSVReader& reader) {
   string token;
   while (!reader.eol() && (reader.peek() != ",")) {
      token += reader.nextToken();
   }
   transform(token.begin(),token.end(), token.begin(),::tolower);
   value = (token == "true") || (token == "1");
}
//--------------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------
