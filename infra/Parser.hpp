#pragma once
//--------------------------------------------------------------------------------
#include <string_view>
#include <cassert>
#include <cstring>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
//--------------------------------------------------------------------------------
namespace infra {
//--------------------------------------------------------------------------------
struct Parser {
   const char* ptr;
   const char* limit;
   const bool treatNlAsWs;
   unsigned line = 1;
   unsigned pos = 1;
   Parser(const char* ptr, const char* limit, bool treatNlAsWs = false) : ptr{ptr}, limit{limit}, treatNlAsWs{treatNlAsWs} {}
   static uint64_t parseNumber(std::string_view str);
   static std::optional<uint64_t> tryParseNumber(std::string_view str) noexcept;
   static std::optional<double> tryParseDouble(std::string_view str) noexcept;
   static bool isNumber(char c) { return c >= '0' && c <= '9'; }
   static bool isLetter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');}
   static char normalizeLetter(char c) { return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c; }
   static bool isLexemeBeginChar(char c) {
      return (c >= 'a' && c<= 'z') || (c>= 'A' && c<= 'Z') || (c == '_');
   }
   static bool isLexemeChar(char c) {
      return isLexemeBeginChar(c) || isNumber(c);
   }
   static bool isWsChar(char c) { return c == ' ' || c == '\t'; }
   static std::vector<std::string> split(std::string_view input, char sep);
   void skipWs() {
      while (!eof()) {
         if (isWsChar(*ptr)) {
            advance();
            continue;
         }
         if (treatNlAsWs && eol()) {
            advanceLine();
            continue;
         }
         break;
      }
   }
   void advanceLine() { assert(eol()); ++line; pos=1; ++ptr; }
   void advance() { ++pos; ++ptr; }
   static bool isLiteralDelimiter(char c) { return c == '"'; }
   char cur() const { return *ptr; }
   bool eof() const { assert(ptr <= limit); return ptr == limit; }
   bool eol() const { assert(ptr < limit); return *ptr == '\n'; }
   void skipLine() {
      while (!eof() && !eol()) {
         advance();
      }
      if (!eof()) advanceLine();
   }
   std::string_view nextToken();
   std::string_view peek();
   void expect(std::string_view expected);
   const char* getPtr() { return ptr; }
};
//--------------------------------------------------------------------------------
/// Only for uint64_t atm
template <uint64_t mag>
struct UnitInterpreter {
   static bool machineReadable;
   static constexpr uint64_t magnitude = mag;
   static constexpr char units[] = {' ','k','m','g','t','p','e'};
   static bool parse(const char* str,uint64_t& val);
   static void print(std::ostream& out, const uint64_t& val);
   static std::string print(uint64_t val) {
      std::stringstream str;
      print(str,val);
      return str.str();
   }
};
//--------------------------------------------------------------------------------
using BinaryUnitInterpreter = UnitInterpreter<1024>;
using DecimalUnitInterpreter = UnitInterpreter<1000>;
extern template struct UnitInterpreter<1024>;
extern template struct UnitInterpreter<1000>;
//--------------------------------------------------------------------------------
struct TimeInterpreter {
   static constexpr const char* units[] = {"ms","s","m","h"};
   static constexpr uint64_t factors[] = {1000,60,60};
   static bool parse(const char* str, uint64_t& val);
   static void print(std::ostream& out, const uint64_t& val);
   static std::string print(uint64_t val) {
      std::stringstream str;
      print(str,val);
      return str.str();
   }
};
//--------------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------
