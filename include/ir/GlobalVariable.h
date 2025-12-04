#pragma once

#include "ir/Constant.h"

#include <memory>
#include <string>

namespace ir {

class GlobalVariable : public Value {
public:
  GlobalVariable(TypePtr valueType, std::string name, bool isConst,
                 std::shared_ptr<Constant> initializer);

  std::string asOperand() const override { return '@' + name_; }

  const TypePtr &getValueType() const { return valueType_; }
  bool isConst() const { return isConst_; }
  const std::shared_ptr<Constant> &getInitializer() const {
    return initializer_;
  }

  std::string print() const;

private:
  TypePtr valueType_;
  bool isConst_;
  std::shared_ptr<Constant> initializer_;
};

} // namespace ir
