#pragma once
//--------------------------------------------------------------------------------
#include "leandb/infra/util/ScopeGuard.hpp"
#include "leandb/infra/util/Parser.hpp"
#include <cassert>
#include <sstream>
#include <stack>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>
//--------------------------------------------------------------------------------
namespace leandb::infra::util {
struct Timestamp;
struct Timer;
struct TimerDiff;
//--------------------------------------------------------------------------------
enum class JSONStage { AfterKey, AfterValue, First };
//--------------------------------------------------------------------------------
enum class JSONState { Object, Array };
//--------------------------------------------------------------------------------
struct JSONKey {
   std::string_view name;
};
//--------------------------------------------------------------------------------
struct JSONBytes {
   uint64_t bytes;
};
//--------------------------------------------------------------------------------
static inline JSONKey key(std::string_view name) { return {name}; }
//--------------------------------------------------------------------------------
static inline JSONBytes bytes(uint64_t bytes) { return {bytes}; }
//--------------------------------------------------------------------------------
template<bool T> class JSONWriterBase;
//--------------------------------------------------------------------------------
template<> class JSONWriterBase<false> {
  public:
   template<typename T> JSONWriterBase operator<<(const T&) { return *this; }
   void beginObject() {}
   void endObject() {}
   void beginArray() {}
   void endArray() {}
   const char* to_string() const { return ""; }
   auto& getRaw() { return *this; }
};
//--------------------------------------------------------------------------------
using EmptyJSONWriter = JSONWriterBase<false>;
//--------------------------------------------------------------------------------
template<> class JSONWriterBase<true> {
  protected:
   std::stringstream stream;
   std::stack<JSONState> states;
   JSONStage stage=JSONStage::First;

  public:
   JSONWriterBase()=default;
   JSONWriterBase(JSONWriterBase&& other) : stream{std::move(other.stream)},states{std::move(other.states)},stage{other.stage} {}
   template <typename T> JSONWriterBase& operator<<(T data) {
      // Raw values are only allowed after a key or in an array
      assert(states.top() == JSONState::Array || stage == JSONStage::AfterKey);
      if (stage == JSONStage::AfterValue) {
         printValueSeparator();
      }
      stream << '"' << data << '"';
      stage = JSONStage::AfterValue;
      return *this;
   }
   JSONWriterBase& operator<<(bool data) {
      // Raw values are only allowed after a key or in an array
      assert(states.top() == JSONState::Array || stage == JSONStage::AfterKey);
      if (stage == JSONStage::AfterValue) {
         printValueSeparator();
      }
      stream << (data ? "true" : "false");
      stage = JSONStage::AfterValue;
      return *this;
   }
   void printValueSeparator() { stream << ","; }
   JSONWriterBase& operator<<(JSONKey key);
   JSONWriterBase& operator<<(JSONBytes key);
   JSONWriterBase& operator<<(const Timestamp& ts);
   JSONWriterBase& operator<<(const Timer& timer);
   JSONWriterBase& operator<<(const TimerDiff& timer);
   std::stringstream& getRaw() { return stream; }
   void beginObject();
   void endObject();
   void beginArray();
   void endArray();
   std::string to_string() const;
};
//--------------------------------------------------------------------------------
using JSONWriter = JSONWriterBase<true>;
//--------------------------------------------------------------------------------
class JSONReader : public Parser {
   std::string_view input;

   std::stack<JSONState> states;
   JSONStage stage=JSONStage::First;

  public:
  JSONReader(std::string_view input) : Parser{input.data(), input.data() + input.size(), true/*nlAsWs*/} {}

  void consumeOpenObject();

  bool isCloseObject() { return peek() == "}"; }
  void consumeCloseObject();

  void consumeOpenArray() { expect("["); }
  void consumeCloseArray() { expect("]"); }
  bool isCloseArray() { return peek() == "]"; }
  void consumeOptionalComma() {
     if (peek() == ",") { expect(","); }
  }
  void skipValue();
  bool isNull();
  std::string_view consumeKey();
};
//--------------------------------------------------------------------------------
struct JSONObject;
struct JSONValueBase {
   std::string name;
   bool isNull = true;
   JSONValueBase(JSONObject* obj, std::string_view name);
   JSONValueBase() = default;
   virtual ~JSONValueBase() = default;
   virtual void parse(JSONReader& reader) = 0;
   virtual void print(std::ostream& out, unsigned level = 0) const = 0;
};
//--------------------------------------------------------------------------------
struct JSONNumber : public JSONValueBase {
  double value;
  JSONNumber(JSONObject* obj, std::string_view name) : JSONValueBase{obj,name} {}
  void parse(JSONReader& reader) override;
  int64_t getInt() const;
  uint64_t getUInt() const;
  bool isInt() const;
  double getDouble() const { return value; }
  double getDoubleOr(double alternative) const;
  void print(std::ostream& out, unsigned level = 0) const override;
};
//--------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const JSONNumber& n);
//--------------------------------------------------------------------------------
struct JSONString : public JSONValueBase {
  std::string value;
  JSONString(JSONObject* obj, std::string_view name) : JSONValueBase{obj,name} {}
  void parse(JSONReader& reader) override;
  std::string get() const { return value; }
  void print(std::ostream& out, unsigned level = 0) const override;
};
//--------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const JSONString& s);
//--------------------------------------------------------------------------------
struct JSONBool : public JSONValueBase {
  bool value;
  JSONBool(JSONObject* obj, std::string_view name) : JSONValueBase{obj, name} {}
  void parse(JSONReader& reader) override;
  bool get() const { return value; }
  bool isTrue() const { return !isNull && value; }
  bool isFalse() const { return !isNull && !value; }
  void print(std::ostream& out, unsigned level = 0) const override;
};
//--------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const JSONBool& s);
//--------------------------------------------------------------------------------
template <typename T>
struct JSONArray : public JSONValueBase {
   std::vector<std::unique_ptr<T>> objects;
   JSONObject* obj;
   const T& operator[](size_t i) const { return *objects[i]; }
   JSONArray(JSONObject* obj, std::string_view name) : JSONValueBase{obj, name}, obj{obj} {}

   void parse(JSONReader& reader) override {
      reader.consumeOpenArray();
      while (!reader.isCloseArray()) {
         objects.emplace_back(std::make_unique<T>());
         objects.back()->parse(reader);
         reader.consumeOptionalComma();
      }
      reader.consumeCloseArray();
   }
   struct iter {
      const JSONArray* arr;
      uint64_t idx;
      bool operator!=(const iter& other) const { return (arr != other.arr) || (idx != other.idx); }
      void operator++() { ++idx; }
      const T& operator*() { return *arr->objects[idx]; }
   };
   iter begin() const { return {this, 0}; }
   iter end() const { return {this, objects.size()}; }
   void print(std::ostream& out, unsigned level = 0) const override {
      out << "[\n";
      for (auto& o : objects) {
         out << std::string(level * 3, ' ');
         o->print(out, level + 1);
         out << ",\n";
      }
      out << std::string((level ? level - 1 : 0) * 3, ' ');
      out << "]";
   }
};
//--------------------------------------------------------------------------------
struct JSONObject : public JSONValueBase {
   std::unordered_map<std::string_view, JSONValueBase*> members;
   JSONObject(JSONObject* obj, std::string_view name) : JSONValueBase{obj, name} {}
   JSONObject() : JSONValueBase{} {}
   void parse(JSONReader& reader) override;
   void registerNode(JSONValueBase* node, std::string_view name);
   void print(std::ostream& out, unsigned level = 0) const override;
};
//--------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const JSONObject& s);
//--------------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------
