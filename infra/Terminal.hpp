#pragma once
//--------------------------------------------------------------------------------
#include <memory>
//--------------------------------------------------------------------------------
namespace infra {
//--------------------------------------------------------------------------------
class Terminal {
public:

  static constexpr char RED[] = "\033[0;31m";
  static constexpr char GREY[] = "\033[0;97m";
  static constexpr char DARKGREY[] = "\033[0;90m";
  static constexpr char BLUE[] = "\033[0;94m";
  static constexpr char GREEN[] = "\033[0;32m";
  static constexpr char NOCOLOR[] = "\033[0m";

};
//--------------------------------------------------------------------------------
}
//--------------------------------------------------------------------------------
