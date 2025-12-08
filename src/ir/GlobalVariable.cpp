// 全局变量节点的打印实现
#include "ir/GlobalVariable.h"

#include <sstream>
#include <utility>

namespace ir {

GlobalVariable::GlobalVariable(TypePtr valueType, std::string name, bool isConst,
                               std::shared_ptr<Constant> initializer)
    : Value(Type::getPointer(valueType), ValueKind::Global, std::move(name)),
      valueType_(std::move(valueType)), isConst_(isConst),
      initializer_(std::move(initializer)) {}

std::string GlobalVariable::print() const {
  std::ostringstream oss;
  oss << asOperand() << " = dso_local "
      << (isConst_ ? "constant " : "global ") << valueType_->str() << ' ';
  if (initializer_) {
    oss << initializer_->asOperand();
  } else {
    oss << "zeroinitializer";
  }
  return oss.str();
}

} // namespace ir
