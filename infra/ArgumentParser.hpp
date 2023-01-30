#pragma once
//--------------------------------------------------------------------------------
#include <iostream>
#include <memory>
#include <sstream>
#include <typeinfo>
#include <optional>
#include <unordered_map>
#include <vector>
//--------------------------------------------------------------------------------
namespace infra {
//--------------------------------------------------------------------------------
struct ShortName {
  std::optional<std::string> name;
  explicit ShortName() = default;
  explicit ShortName(std::string s) : name{s} {}
};
//--------------------------------------------------------------------------------
struct ArgumentBase {
   ShortName shortPrefix;
   std::string longPrefix;
   std::string description;
   bool valueSet;

   ArgumentBase(ShortName shortPrefix, std::string longPrefix, std::string description)
      : shortPrefix{shortPrefix},
        longPrefix{longPrefix},
        description{description},
        valueSet{false} {}

   virtual bool isPosArg() const { return false; }
   virtual bool parse(const char* value) = 0;
   virtual std::string getType() const = 0;
   virtual bool isRequired() const = 0;
   virtual bool hasValue() const { return true; }
   std::string getLongName() const { return longPrefix; }
   std::string getShortName() const { return *shortPrefix.name; }
   bool hasShortName() const { return shortPrefix.name.has_value(); }
   std::string getDescription() const { return description; }
   bool isInitialized() const {
      return !isRequired() || valueSet;
   }
   bool isValueSet() const { return valueSet; }
   virtual std::string defaultValueAsString() const = 0;
   virtual std::string getValue() const = 0;
};
//--------------------------------------------------------------------------------
class ArgumentParser {
   std::vector<ArgumentBase*> arguments;
   std::vector<ArgumentBase*> posArgs;
   ArgumentBase* varArg;
   std::unordered_map<std::string, ArgumentBase*> shortArgs;
   std::unordered_map<std::string, ArgumentBase*> longArgs;
   std::string executable;

   std::string getArgumentDetails(const ArgumentBase* arg);

   public:
   ArgumentParser() : varArg{nullptr} {}
   auto begin() const { return arguments.begin(); }
   auto end() const { return arguments.end(); }
   bool addArgument(ArgumentBase* arg);
   bool addPositionalArgument(ArgumentBase* arg);
   bool addVariableArgument(ArgumentBase* arg);
   std::string getConfiguration();
   void dumpSchema(std::ostream& stream) const;
   void dumpArgs(std::ostream& stream) const;
   std::string getHelp();
   bool parse(int argc, char** argv);
};
//--------------------------------------------------------------------------------
template <typename T>
class TypedArgument : public ArgumentBase {
  protected:
   T value;

   public:
   TypedArgument(ShortName shortPrefix, std::string&& longPrefix, std::string&& description)
      : ArgumentBase{std::move(shortPrefix), std::move(longPrefix), std::move(description)} {}

   bool parse(const char* argv) override {
      if constexpr (std::is_same_v<T, std::string>) {
        value = argv;
        valueSet = true;
        return true;
      } else {
         std::stringstream stream(argv);
         stream >> value;
         if (!stream.fail()) {
            valueSet = true;
         }
         return !stream.fail();
      }
   }
   operator T() {
      return value;
   }
   T get() const {
      return value;
   }
   bool hasValue() const override { return true; }
   std::string getType() const override { return ""; }
   std::string getValue() const override { return std::to_string(value); }
};
//--------------------------------------------------------------------------------
template <typename T>
class OptionalArgument : public TypedArgument<T> {
   T defaultValue;

   public:
   OptionalArgument(ArgumentParser* parser, ShortName shortPrefix, std::string&& longPrefix, std::string&& description, T defaultValue = T{})
      : TypedArgument<T>{std::move(shortPrefix), std::move(longPrefix), std::move(description)}, defaultValue{defaultValue} {
      this->value = defaultValue;
      parser->addArgument(this);
   }
   OptionalArgument(ArgumentParser* parser, std::string longPrefix, std::string&& description, T defaultValue = T{})
      : TypedArgument<T>{ShortName{}, std::move(longPrefix), std::move(description)}, defaultValue{defaultValue} {
      this->value = defaultValue;
      parser->addArgument(this);
   }
   std::string defaultValueAsString() const override { return std::to_string(defaultValue);   }
   bool isRequired() const override { return false; }
};
//--------------------------------------------------------------------------------
template <typename T>
class PositionalArgument : public TypedArgument<T> {
   bool required;
   public:
   PositionalArgument(ArgumentParser* parser, std::string longPrefix, std::string description, bool required = true) : TypedArgument<T>{ShortName{}, std::move(longPrefix), std::move(description)}, required{required} {
      parser->addPositionalArgument(this);
   }
   bool isRequired() const override { return required; }
   std::string defaultValueAsString() const override { return "<none>";   }
};
//--------------------------------------------------------------------------------
// An argument that can take a list. Must be the last argument to work correctly
template<typename T>
class VariableArgument : public TypedArgument<T> {
   std::vector<T> values;
  public:
  VariableArgument(ArgumentParser* parser, std::string longPrefix, std::string description) : TypedArgument<T>{ShortName{}, std::move(longPrefix), std::move(description)} {
      parser->addVariableArgument(this);
  }

   bool parse(const char* argv) override {
      bool result = TypedArgument<T>::parse(argv);
      if (result) {
         values.push_back(this->value);
      }
      return result;
   }
   bool isRequired() const override { return false; }
   std::string getValue() const override {
      std::string result;
      for (uint64_t i = 0; i < values.size();++i) {
         result += TypedArgument<T>::getValue();
         if (i+1 != values.size()) {
            result += ", ";
         }
      }
      return result;
   }
   auto begin() { return values.begin(); }
   auto end() { return values.end(); }
   auto size() const { return values.size(); }
   std::string defaultValueAsString() const override{ return "<emptyList>"; }
};
//--------------------------------------------------------------------------------
template <>
bool TypedArgument<bool>::hasValue() const;
template <>
std::string TypedArgument<std::string>::getType() const;
template <>
std::string TypedArgument<double>::getType() const;
template <>
std::string TypedArgument<std::string>::getValue() const;
template <>
std::string TypedArgument<uint64_t>::getType() const;
template <>
std::string TypedArgument<unsigned>::getType() const;
template <>
std::string OptionalArgument<std::string>::defaultValueAsString() const;
//--------------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------
