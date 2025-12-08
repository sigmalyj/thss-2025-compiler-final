// IR 所有值的基类实现
#include "ir/Value.h"

namespace ir {

Value::Value(TypePtr type, ValueKind kind, std::string name)
    : type_(std::move(type)), name_(std::move(name)), kind_(kind) {}

} // namespace ir
