#pragma once

#include "ir/Type.h"
#include "ir/Value.h"

#include <memory>
#include <vector>

namespace ir {

class Constant : public Value {
public:
  Constant(TypePtr type, std::string name = "")
      : Value(std::move(type), ValueKind::Constant, std::move(name)) {}
  ~Constant() override = default;
};

class ConstantInt : public Constant {
public:
  static std::shared_ptr<ConstantInt> get(int value);

  int getValue() const { return value_; }
  std::string asOperand() const override;

private:
  explicit ConstantInt(int value);

  int value_;
};

class ConstantZero : public Constant {
public:
  explicit ConstantZero(TypePtr type);

  std::string asOperand() const override { return "zeroinitializer"; }
};

class ConstantNull : public Constant {
public:
  ConstantNull(TypePtr type) : Constant(std::move(type)) {}

  std::string asOperand() const override { return "null"; }
};

class ConstantArray : public Constant {
public:
  ConstantArray(ArrayTypePtr type, std::vector<std::shared_ptr<Constant>> elems);

  const std::vector<std::shared_ptr<Constant>> &getElements() const {
    return elements_;
  }

  std::string asOperand() const override;

private:
  std::vector<std::shared_ptr<Constant>> elements_;
};

} // namespace ir
