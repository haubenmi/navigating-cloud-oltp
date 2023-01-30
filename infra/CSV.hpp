#pragma once
#include "Parser.hpp"
#include <cstdint>
#include <string_view>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
//--------------------------------------------------------------------------------
namespace infra {
//--------------------------------------------------------------------------------
struct CSVReader : public Parser {
  std::string_view input;
  CSVReader(std::string_view input) : Parser{input.data(), input.data() + input.size(), false/*nlAsWs*/} {}

};
//--------------------------------------------------------------------------------
struct CSVValueBase;
struct CSVBase {
   std::unordered_map<std::string_view, CSVValueBase*> mapping;
   void registerField(CSVValueBase* v, std::string_view name) { mapping[name] = v; }
};
struct CSVSchema {
  CSVBase* base;
  CSVSchema(CSVBase* base) : base{base} {}
};
//--------------------------------------------------------------------------------
struct CSVValueBase {
   std::string name;
   CSVValueBase(CSVSchema* schema, std::string_view name);

   virtual void parse(CSVReader& reader) = 0;
};
template <typename T>
struct CSV : public CSVBase {
   std::vector<T> values;
   void parse(CSVReader& reader) {
      std::unordered_map<unsigned, std::string> fieldmapping;
      unsigned fields = 0;
      while (!reader.eol()) {
        std::string name;
        while (!reader.eol() && (reader.peek() != ",")) {
          name += reader.nextToken();
        }
        fieldmapping[fields] = name;
        if (!reader.eol()) reader.expect(",");
        ++fields;
      }
      auto checkFieldMappings = [&]() {
         // Check that all mapped fields are actually present
         std::unordered_set<std::string_view> existingFields;
         for (auto& f : fieldmapping) {
            existingFields.insert(f.second);
         }
         for (auto& m : mapping) {
            if (!existingFields.contains(m.first)) {
               throw std::runtime_error("Mapped field '" + std::string(m.first) + "' is not present in csv file");
            }
         }
      };

      bool firstLine = true;
      reader.advanceLine();
      while (!reader.eof()) {
         values.emplace_back(this);
         if (firstLine) {
           checkFieldMappings();
           firstLine = false;
         }
         for (unsigned i = 0; i < fields; ++i) {
            auto fieldname = fieldmapping[i];
            auto iter = mapping.find(fieldname);
            if (iter != mapping.end()) {
              iter->second->parse(reader);
            } else {
               // Consume the single token
               while (!reader.eol() && (reader.peek() != ",")) {
                  reader.nextToken();
               }
            }
            if ((i + 1) != fields) {
               reader.expect(",");
            }
         }
         if (!reader.eof()) reader.advanceLine();
      }
   }

   auto begin() const { return values.begin(); }
   auto end() const { return values.end(); }
};
//--------------------------------------------------------------------------------
struct CSVNumber : public CSVValueBase {
   double value;
   CSVNumber(CSVSchema* csv, std::string_view name) : CSVValueBase{csv, name} {}
   void parse(CSVReader& reader) override;
   int64_t getInt() const;
   uint64_t getUInt() const;
   bool isInt() const;
   double getDoubleOr(double alternative) const;
   double getDouble() const { return value; }
};
//--------------------------------------------------------------------------------
struct CSVString : public CSVValueBase {
   std::string value;
   CSVString(CSVSchema* csv, std::string_view name) : CSVValueBase{csv, name} {}
   void parse(CSVReader& reader) override;
   const std::string& get() const { return value; }
};
//--------------------------------------------------------------------------------
struct CSVBool : public CSVValueBase {
   bool value = false;
   CSVBool(CSVSchema* csv, std::string_view name) : CSVValueBase{csv, name} {}
   void parse(CSVReader& reader) override;
   bool get() const { return value; }
};
//--------------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------
