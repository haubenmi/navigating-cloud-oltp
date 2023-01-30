#include "leandb/infra/util/JSON.hpp"
#include "leandb/infra/Exception.hpp"
#include "leandb/infra/util/Parser.hpp"
#include "leandb/infra/util/Timer.hpp"
#include "leandb/infra/util/Timestamp.hpp"
#include <iomanip>
//--------------------------------------------------------------------------------
using namespace std;
using namespace leandb::infra;
//--------------------------------------------------------------------------------
namespace leandb::infra::util {
//--------------------------------------------------------------------------------
void JSONWriterBase<true>::beginObject() {
   if (stage == JSONStage::AfterValue) {
      printValueSeparator();
   }
   states.push(JSONState::Object);
   stage = JSONStage::First;
   stream << '{';
}
//--------------------------------------------------------------------------------
void JSONWriterBase<true>::endObject() {
   assert(states.top() == JSONState::Object);
   states.pop();
   stream << '}';
   stage = JSONStage::AfterValue;
}
//--------------------------------------------------------------------------------
void JSONWriterBase<true>::beginArray() {
   if (stage == JSONStage::AfterValue) {
      printValueSeparator();
   }
   states.push(JSONState::Array);
   stage = JSONStage::First;
   stream << '[';
}
//--------------------------------------------------------------------------------
void JSONWriterBase<true>::endArray() {
   assert(states.top() == JSONState::Array);
   states.pop();
   stream << ']';
   stage = JSONStage::AfterValue;
}
//--------------------------------------------------------------------------------
JSONWriterBase<true>& JSONWriterBase<true>::operator<<(JSONKey key) {
   // Keys are not allowed after keys
   assert(stage != JSONStage::AfterKey);
   if (stage == JSONStage::AfterValue) {
      printValueSeparator();
   }
   stream << "\33[1m\"" << key.name << "\"\33[m";
   stream << ':';
   stage = JSONStage::AfterKey;
   return *this;
}
//--------------------------------------------------------------------------------
JSONWriterBase<true>& JSONWriterBase<true>::operator<<(JSONBytes key) {
   stringstream str;
   BinaryUnitInterpreter::print(str, key.bytes);
   this->operator<<(str.str());
   return *this;
}
//--------------------------------------------------------------------------------
JSONWriterBase<true>& JSONWriterBase<true>::operator<<(const Timestamp& ts) {
   auto time = ts.to_time_t();
   tm timeStruct;
   stream << '"' << put_time(localtime_r(&time, &timeStruct), "%F %T") << "." << setfill('0') << setw(3) << ts.getMilliseconds() << '"';
   stage = infra::util::JSONStage::AfterValue;
   return *this;
}
//--------------------------------------------------------------------------------
JSONWriterBase<true>& JSONWriterBase<true>::operator<<(const Timer& timer) {
   stream << "\33[4m";
   (*this) << (std::to_string(timer.elapsed()) + "ms\33[m");
   return *this;
}
//--------------------------------------------------------------------------------
JSONWriterBase<true>& JSONWriterBase<true>::operator<<(const TimerDiff& timer) {
   stream << "\33[4m";
   (*this) << (std::to_string(timer.data) + "ms\33[m");
   return *this;
}
//--------------------------------------------------------------------------------
string JSONWriterBase<true>::to_string() const { return stream.str(); }
//--------------------------------------------------------------------------------
void JSONReader::consumeOpenObject() {
   expect("{");
   states.push(JSONState::Object);
   stage = JSONStage::First;
}
//--------------------------------------------------------------------------------
void JSONReader::consumeCloseObject() {
   expect("}");
   assert(states.top() == JSONState::Object);
   states.pop();
   stage = JSONStage::AfterValue;
}
//--------------------------------------------------------------------------------
string_view JSONReader::consumeKey() {
   auto key = nextToken();
   expect(":");
   stage = JSONStage::AfterKey;
   return key;
}
//--------------------------------------------------------------------------------
bool JSONReader::isNull() { return peek() == "null"; }
//--------------------------------------------------------------------------------
void JSONReader::skipValue() {
   // Either null, number, string, array or object
   //   auto nextVal = peek();
   assert(stage == JSONStage::AfterKey);
   auto nextVal = peek();
   if (nextVal == "{") {
      consumeOpenObject();
      while (!isCloseObject()) {
         consumeKey();
         skipValue();
         consumeOptionalComma();
      }
      consumeCloseObject();
   } else if (nextVal == "[") {
      consumeOpenArray();
      while (!isCloseArray()) {
         skipValue();
         consumeOptionalComma();
      }
      consumeCloseArray();
   } else {
      // An atomic value (including null) can just be skipped by consuming one token
      nextToken();
   }
}
//--------------------------------------------------------------------------------
JSONValueBase::JSONValueBase(JSONObject* obj, string_view n) : name{n} { obj->registerNode(this, name); }
//--------------------------------------------------------------------------------
void JSONNumber::parse(JSONReader& reader) {
   auto token = reader.nextToken();
   if (token != "null") {
      if (auto d = Parser::tryParseDouble(token)) {
         value = *d;
         isNull = false;
      } else {
        isNull = true;
      }
   } else {
      isNull = true;
   }
   //   cerr << "JSONInt: " << value << "\n";
}
//--------------------------------------------------------------------------------
bool JSONNumber::isInt() const {
  int64_t i = value;
  return static_cast<double>(i) == value;
}
//--------------------------------------------------------------------------------
int64_t JSONNumber::getInt() const {
   int64_t result = value;
   if (static_cast<double>(result) == value) {
      return result;
   } else {
      throw domain_error("json number cannot be represent as an integer");
   }
}
//--------------------------------------------------------------------------------
uint64_t JSONNumber::getUInt() const {
   uint64_t result = value;
   if (static_cast<double>(result) == value) {
      return result;
   } else {
      throw domain_error("json number cannot be represent as an unsigned integer");
   }
}
//--------------------------------------------------------------------------------
double JSONNumber::getDoubleOr(double alternative) const {
   return isNull ? alternative : value;
}
//--------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const JSONNumber& n) {
   n.print(out);
   return out;
}
//--------------------------------------------------------------------------------
void JSONNumber::print(ostream& out, unsigned) const {
   if (isNull) {
      out << "null";
   } else if (isInt()) {
      out << getInt();
   } else {
      out << value;
   }
}
//--------------------------------------------------------------------------------
void JSONString::parse(JSONReader& reader) {
   if (reader.isNull()) {
      reader.nextToken();
      isNull = true;
   } else {
      value = reader.nextToken();
      isNull = false;
   }
   //   cerr << "JSONString : " << value << "\n ";
}
//--------------------------------------------------------------------------------
void JSONString::print(ostream& out, unsigned) const {
   if (isNull) {
      out << "null";
   } else {
      out << '"' << get() << '"';
   }
}
//--------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const JSONString& s) {
   s.print(out);
   return out;
}
//--------------------------------------------------------------------------------
void JSONBool::parse(JSONReader& reader) {
   if (reader.isNull()) {
      reader.nextToken();
      isNull = true;
   } else {
      auto token = reader.nextToken();
      value = (token == "true") || (token == "1");
      isNull = false;
   }
   //   cerr << "JSONBool : " << value << "\n ";
}
//--------------------------------------------------------------------------------
void JSONBool::print(ostream& out, unsigned) const { out << (isNull ? "null" : (value ? "true" : "false")); }
//--------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const JSONBool& b) {
   b.print(out);
   return out;
}
//--------------------------------------------------------------------------------
void JSONObject::registerNode(JSONValueBase* node, std::string_view name) {
   assert(!members.count(name));
   //   cout << "register: " << name << " at address " << reinterpret_cast<const void*>(name.data()) << "\n";
   members[name] = node;
}
//--------------------------------------------------------------------------------
void JSONObject::parse(JSONReader& reader) {
   if (reader.isNull()) {
      isNull = true;
      reader.nextToken();
      return;
   }
   reader.consumeOpenObject();
   while (!reader.isCloseObject()) {
      auto key = reader.consumeKey();
      auto iter = members.find(key);
      if (iter != members.end()) {
         //         cerr << "JSONKey: " << key << "\n";
         iter->second->parse(reader);
      } else {
         reader.skipValue();
         //                throw infra::Exception(infra::Exception::ErrorCode::InternalError, "Unexpected key in JSON object: "s + string{key});
      }
      reader.consumeOptionalComma();
   }
   reader.consumeCloseObject();
   isNull = false;
}
//--------------------------------------------------------------------------------
std::ostream& operator<<(std::ostream& out, const JSONObject& o) {
   o.print(out);
   return out;
}
//--------------------------------------------------------------------------------
void JSONObject::print(ostream& out, unsigned level) const {
   out << "{\n";
   for (auto& m : members) {
      out << std::string(level * 3, ' ');
      out << m.first << ": ";
      m.second->print(out, level + 1);
      out << ",\n";
   }
   out << std::string((level ? level - 1 : 0) * 3, ' ');
   out << "}";
}
//--------------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------
