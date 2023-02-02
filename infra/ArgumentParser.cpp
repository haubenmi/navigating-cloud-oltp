#include "ArgumentParser.hpp"
#include <iostream>
//--------------------------------------------------------------------------------
using namespace std;
//--------------------------------------------------------------------------------
namespace infra {
//--------------------------------------------------------------------------------
bool ArgumentParser::addArgument(ArgumentBase* arg) {
   if (arg->hasShortName()) {
     if (shortArgs.count(arg->getShortName())) {
         cerr << "conflict adding argument " << arg->longPrefix << endl;
         return false;
      }
      shortArgs[arg->getShortName()] = arg;
   }
   if (longArgs.count(arg->getLongName())) {
      cerr << "conflict adding argument " << arg->getLongName() << endl;
      return false;
   }
   longArgs[arg->getLongName()] = arg;
   arguments.push_back(arg);
   return true;
}
//--------------------------------------------------------------------------------
bool ArgumentParser::addPositionalArgument(ArgumentBase* arg) {
   posArgs.push_back(arg);
   return true;
}
//--------------------------------------------------------------------------------
bool ArgumentParser::addVariableArgument(ArgumentBase* arg) {
   if (varArg) {
      // There can only be one variable argument
      return false;
   }
   varArg = arg;
   return true;
}
//--------------------------------------------------------------------------------
string ArgumentParser::getConfiguration() {
   string result;
   result += "####### Current Arguments for " + executable + "######\n";
   for (auto arg : arguments) {
      result += "#\t" + arg->getLongName() + "\t=\t" + arg->getValue() + "\t\n";
   }
   result += "############################################\n";
   return result;
}

//--------------------------------------------------------------------------------
void ArgumentParser::dumpSchema(ostream& stream) const {
   bool komma = false;
   for (auto& arg : arguments) {
      if (komma) {
         stream << ",";
      } else {
         komma = true;
      }
      stream << arg->longPrefix;
   }
   stream << endl;
}
//--------------------------------------------------------------------------------
void ArgumentParser::dumpArgs(ostream& stream) const {
   bool komma = false;
   for (auto& arg : arguments) {
      if (komma) {
         stream << ",";
      } else {
         komma = true;
      }
      stream << arg->getValue();
   }
   stream << endl;
}
//--------------------------------------------------------------------------------
string ArgumentParser::getArgumentDetails(const ArgumentBase* arg) {
   string result = " ";
   if (arg->hasShortName()) {
     result += " -" + arg->getShortName() + ",";
   }
   result += " --"s + (!arg->hasValue() ? "[no-]" : "") + arg->getLongName() + "\t\t" + arg->getDescription();
   if (!arg->isRequired()) {
      result += " (default = " + arg->defaultValueAsString() + ")";
   }
   result += "\n";
   return result;
}
//--------------------------------------------------------------------------------
string ArgumentParser::getHelp() {
   auto result = "Usage: " + executable;
   for (auto arg : arguments) {
      if (arg->hasValue()) {
         result += " --" + arg->getLongName() + " <" + arg->getType() + ">";
      } else {
         result += " --[no-]" + arg->getLongName();
      }
   }
   for (auto posArg : posArgs) {
      result += " " + posArg->getLongName();
   }
   result += "\n";
   for (auto arg : arguments) {
      result += getArgumentDetails(arg);
   }
   return result;
}
//--------------------------------------------------------------------------------
bool ArgumentParser::parse(int argc, char** argv) {
   if (argc > 0) {
      executable = argv[0];
   }
   int i = 1;
   unsigned currentPosArg = 0;
   while (i < argc) {
      string arg{argv[i]};
      ArgumentBase* argp = nullptr;
      bool negate = false;
      // Check for short optional args
      if (arg.size() >= 2 && arg[0] == '-' && arg[1] != '-') {
         // Assume we got short prefix
         argp = shortArgs[arg.substr(1)];
         ++i;
      } else if (arg.size() >= 3 && arg[0] == '-' && arg[1] == '-') {
         // Assume we got long prefix
         if (arg.size() > string("--no-").size() && arg[2] == 'n' && arg[3] == 'o' && arg[4] == '-') {
            negate = true;
            argp = longArgs[arg.substr(5)];
         } else {
            argp = longArgs[arg.substr(2)];
         }
         ++i;
      } else if (currentPosArg < posArgs.size()) {
         argp = posArgs[currentPosArg++];
      } else if (varArg) {
         argp = varArg;
      }
      if (!argp) {
         cerr << "invalid argument: " << arg << endl;
         return false;
      }
      if (argp->hasValue()) {
         if (i >= argc) {
            cerr << "reached end of input while looking for value for argument: " << argp->getLongName() << endl;
            return false;
         }
         char* value = argv[i++];
         if (!argp->parse(value)) {
            cerr << "invalid value for argument `" << argp->getLongName() << "`: " << value << endl;
            return false;
         }
      } else {
         // If it has no value, it is boolean
         argp->parse(negate ? "0" : "1");
      }
   }
   for (auto p : posArgs) {
      if (p->isRequired() && !p->isValueSet()) {
         cerr << "no argument provided for `" << p->getLongName() << "`." << endl;
         return false;
      }
   }
   return true;
}
//--------------------------------------------------------------------------------
template <>
bool TypedArgument<bool>::hasValue() const { return false; }
template <>
std::string TypedArgument<std::string>::getType() const { return "string"; }
template <>
std::string TypedArgument<double>::getType() const { return "double"; }
template <>
std::string TypedArgument<std::string>::getValue() const { return value; }
template <>
std::string TypedArgument<uint64_t>::getType() const { return "uint64_t"; }
template <>
std::string TypedArgument<unsigned>::getType() const { return "unsigned"; }
template <>
std::string OptionalArgument<std::string>::defaultValueAsString() const { return defaultValue; }
//--------------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------
