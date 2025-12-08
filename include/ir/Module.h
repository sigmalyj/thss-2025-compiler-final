// 模块容器声明
#pragma once

#include "ir/Function.h"
#include "ir/GlobalVariable.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace ir {

class Module {
public:
  Module();

  GlobalVariable *addGlobal(const std::string &name, const TypePtr &valueType,
                            bool isConst,
                            std::shared_ptr<Constant> initializer = nullptr);

  Function *createFunction(const std::string &name, FunctionTypePtr type,
                           bool declaration = false);
  Function *getFunction(const std::string &name) const;

  const std::vector<std::unique_ptr<GlobalVariable>> &getGlobals() const {
    return globals_;
  }

  const std::vector<std::unique_ptr<Function>> &getFunctions() const {
    return functions_;
  }

  std::string print() const;

private:
  void declareBuiltins();

  std::vector<std::unique_ptr<GlobalVariable>> globals_;
  std::vector<std::unique_ptr<Function>> functions_;
  std::unordered_map<std::string, Function *> functionMap_;
};

} // namespace ir
