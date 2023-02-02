#include "Parser.hpp"
#include <iomanip>
//--------------------------------------------------------------------------------
using namespace std;
//--------------------------------------------------------------------------------
namespace infra {
//--------------------------------------------------------------------------------
vector<string> Parser::split(string_view input, char sep) {
   vector<string> result;
   string_view::size_type from = 0;
   do {
      auto cur = input.find(sep, from);
      result.emplace_back(input.substr(from,cur-from));
      if (cur != string_view::npos) ++cur;
      from = cur;
   } while (from != string_view::npos);
   return result;
}
//--------------------------------------------------------------------------------
uint64_t Parser::parseNumber(string_view str) {
   uint64_t result = 0;
   for (auto c : str) {
      if (c > '9' || c < '0') throw std::invalid_argument("parseNumber(): Input is not a digit");
      result = 10 * result + (c - '0');
   }
   return result;
}
//--------------------------------------------------------------------------------
optional<uint64_t> Parser::tryParseNumber(string_view str) noexcept {
   uint64_t result = 0;
   for (auto c : str) {
      if (c > '9' || c < '0') return {};
      result = 10 * result + (c - '0');
   }
   return result;
}
//--------------------------------------------------------------------------------
string_view Parser::peek() {
   auto old = ptr;
   auto oldLine = line;
   auto oldPos = pos;
   auto result = nextToken();
   ptr = old;
   line = oldLine;
   pos = oldPos;
   return result;
}
//--------------------------------------------------------------------------------
void Parser::expect(string_view expected) {
   auto actual = nextToken();
   if (expected != actual) {
      throw runtime_error{string{"Unexpected Token found in line:" + to_string(line) + " . Got `"} + string{actual} + "`, expected `" + string{expected} + "`"};
   }
}
//--------------------------------------------------------------------------------
optional<double> Parser::tryParseDouble(string_view str) noexcept {
   try {
      return {stod(string{str})};
   } catch (...) {
      return {};
   }
}
//--------------------------------------------------------------------------------
string_view Parser::nextToken() {
begin:
   skipWs();
   auto tokenBegin = ptr;
   auto tokenEnd = tokenBegin;
   ++ptr;
   if (isLexemeBeginChar(*tokenBegin)) {
      for (; !eof() && isLexemeChar(*ptr); ++ptr) {}
      tokenEnd = ptr;

   } else if (isNumber(*tokenBegin)) {
      for (; !eof() && (isNumber(*ptr) || *ptr == '+' || *ptr == '-' || *ptr == '.'); ++ptr) {}
      tokenEnd = ptr;
   } else if (*tokenBegin == '#') {
      // Comment
      skipLine();
      if (eof()) return {};
      goto begin;
   } else if (*tokenBegin == '/' && !eof() && *ptr == '/') {
      // Comment
      skipLine();
      if (eof()) return {};
      goto begin;
   } else if (isLiteralDelimiter(*tokenBegin)) {
      ++tokenBegin;
      for (; (ptr != limit) && !isLiteralDelimiter(*ptr); ++ptr) {}
      tokenEnd = ptr;
      ++ptr;
   } else {
      tokenEnd = ptr;
   }
   string_view result{tokenBegin, static_cast<size_t>(tokenEnd - tokenBegin)};
   if (result == "\n") {
     ++line;
     pos = 1;
   }
   return result;
}
//--------------------------------------------------------------------------------
template <uint64_t mag>
bool UnitInterpreter<mag>::parse(const char* str, uint64_t& val) {
   auto length = strlen(str);
   char unit = str[length - 1];
   if (Parser::isLetter(unit)) {
      unit = Parser::normalizeLetter(unit);
   }
   auto temp = Parser::parseNumber(std::string_view{str, length - 1});
   unsigned i = 0;
   while (i <= 6) {
      if (units[i] == unit) break;
      ++i;
      temp *= magnitude;
   }
   if (i == 7) {
      return false;
   }
   val = temp;
   return true;
}
//--------------------------------------------------------------------------------
template <uint64_t mag>
bool UnitInterpreter<mag>::machineReadable = false;
//--------------------------------------------------------------------------------
template <uint64_t mag>
void UnitInterpreter<mag>::print(ostream& out, const uint64_t& val) {
   if (machineReadable) {
      out << val;
      return;
   }
   uint64_t temp = val;
   unsigned orders = 0;
   while (temp >= magnitude) {
      temp /= magnitude;
      ++orders;
   }
   uint64_t finalDivisor = 1;
   for(unsigned i = 0; i < orders; ++i) finalDivisor *= magnitude;

   double result = (double)val/finalDivisor;
   assert(orders <= 6);
   stringstream ss;
   int64_t result_int = result;
   if (result_int == result) {
      ss << result_int;
   } else {
      ss << std::fixed << std::setprecision(1) << result;
   }
   if (orders != 0) ss << units[orders];
   out << ss.str();
}
//--------------------------------------------------------------------------------
template struct UnitInterpreter<1024>;
template struct UnitInterpreter<1000>;
//--------------------------------------------------------------------------------
bool TimeInterpreter::parse(const char* str, uint64_t& val) {
   std::string inp{str};
   unsigned i = 0;

   while (i <= 3) {
      std::string ending{units[i]};
      if (0 == inp.compare(inp.length() - ending.length(), ending.length(), ending)) {
         break;
      }
      ++i;
   }
   if (i == 3) return false;

   inp.resize(inp.length() - std::string{units[i]}.length());
   auto temp = Parser::parseNumber(inp);
   unsigned j = 0;
   while (j < i) {
      temp *= factors[j];
      ++j;
   }
   val = temp;
   return true;
}
//--------------------------------------------------------------------------------
void TimeInterpreter::print(std::ostream& out, const uint64_t& val) {
   uint64_t temp = val;
   unsigned i = 0;
   // As long as there is no remainder, we can go down a unit
   while (i <= 2) {
      if (temp < factors[i] || temp % factors[i] != 0) break;
      temp = temp / factors[i];
      ++i;
   }
   out << temp << units[i];
}
//--------------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------
