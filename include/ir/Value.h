#pragma once

#include "ir/Type.h"

#include <memory>
#include <string>

namespace ir {

enum class ValueKind {
  Constant,
  Global,
  Argument,
  Instruction,
  Function
};

class Value {
public:
  Value(TypePtr type, ValueKind kind, std::string name = "");
  virtual ~Value() = default;

  const std::string &getName() const { return name_; }
  void setName(std::string name) { name_ = std::move(name); }

  const TypePtr &getType() const { return type_; }
  ValueKind getKind() const { return kind_; }

  bool hasName() const { return !name_.empty(); }

  virtual std::string asOperand() const = 0;

protected:
  TypePtr type_;
  std::string name_;
  ValueKind kind_;
};

} // namespace ir
