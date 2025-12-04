#include "ir/Module.h"

#include <sstream>
#include <utility>

namespace ir {

Module::Module() { declareBuiltins(); }

GlobalVariable *Module::addGlobal(const std::string &name,
                                  const TypePtr &valueType, bool isConst,
                                  std::shared_ptr<Constant> initializer) {
  auto global = std::make_unique<GlobalVariable>(valueType, name, isConst,
                                                 std::move(initializer));
  GlobalVariable *raw = global.get();
  globals_.push_back(std::move(global));
  return raw;
}

Function *Module::createFunction(const std::string &name, FunctionTypePtr type,
                                 bool declaration) {
  if (auto it = functionMap_.find(name); it != functionMap_.end()) {
    if (!declaration) {
      it->second->setDeclaration(false);
    }
    return it->second;
  }
  auto func = std::make_unique<Function>(name, std::move(type), declaration);
  Function *raw = func.get();
  functions_.push_back(std::move(func));
  functionMap_[name] = raw;
  return raw;
}

Function *Module::getFunction(const std::string &name) const {
  if (auto it = functionMap_.find(name); it != functionMap_.end()) {
    return it->second;
  }
  return nullptr;
}

std::string Module::print() const {
  std::ostringstream oss;
  oss << "; ModuleID = 'sysy'\n";

  for (const auto &global : globals_) {
    oss << global->print() << '\n';
  }

  if (!globals_.empty()) {
    oss << '\n';
  }

  for (const auto &func : functions_) {
    oss << func->print() << '\n';
  }

  return oss.str();
}

void Module::declareBuiltins() {
  auto intTy = Type::getInt32();
  auto voidTy = Type::getVoid();
  auto ptrIntTy = Type::getPointer(intTy);

  createFunction("getint", Type::getFunction(intTy, {}), true);
  createFunction("getch", Type::getFunction(intTy, {}), true);
  createFunction("getarray", Type::getFunction(intTy, {ptrIntTy}), true);
  createFunction("putint", Type::getFunction(voidTy, {intTy}), true);
  createFunction("putch", Type::getFunction(voidTy, {intTy}), true);
  createFunction("putarray", Type::getFunction(voidTy, {intTy, ptrIntTy}),
                 true);
  createFunction("starttime", Type::getFunction(voidTy, {}), true);
  createFunction("stoptime", Type::getFunction(voidTy, {}), true);
}

} // namespace ir
