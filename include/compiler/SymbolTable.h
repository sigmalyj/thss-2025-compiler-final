#pragma once

#include "ir/Module.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace compiler {

struct Symbol {
  ir::Value *address = nullptr;
  ir::TypePtr valueType;
  bool isConst = false;
  bool isGlobal = false;
  bool isFunction = false;
  bool isPointerParam = false;
  std::vector<int> dimensions;
  std::vector<int> constData; // Flattened initializers for const objects
  ir::Function *function = nullptr;
};

class SymbolTable {
public:
  SymbolTable();

  void enterScope();
  void exitScope();

  bool insert(const std::string &name, Symbol symbol);
  Symbol *lookup(const std::string &name);
  const Symbol *lookup(const std::string &name) const;

private:
  std::vector<std::unordered_map<std::string, Symbol>> scopes_;
};

} // namespace compiler
