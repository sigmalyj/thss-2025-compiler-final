#include "compiler/SymbolTable.h"

// 简易作用域符号表，支持嵌套查找与插入

namespace compiler {

SymbolTable::SymbolTable() { enterScope(); }

void SymbolTable::enterScope() { scopes_.emplace_back(); }

void SymbolTable::exitScope() {
  if (!scopes_.empty()) {
    scopes_.pop_back();
  }
}

bool SymbolTable::insert(const std::string &name, Symbol symbol) {
  if (scopes_.empty()) {
    enterScope();
  }
  auto &current = scopes_.back();
  auto [it, inserted] = current.emplace(name, std::move(symbol));
  return inserted;
}

Symbol *SymbolTable::lookup(const std::string &name) {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return &found->second;
    }
  }
  return nullptr;
}

const Symbol *SymbolTable::lookup(const std::string &name) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return &found->second;
    }
  }
  return nullptr;
}

} // namespace compiler
