// 函数及其参数、基本块的声明
#pragma once

#include "ir/BasicBlock.h"
#include "ir/Instruction.h"
#include "ir/Value.h"

#include <memory>
#include <string>
#include <vector>

namespace ir {

class Argument : public Value {
public:
  Argument(TypePtr type, std::string name, class Function *parent);

  Function *getParent() const { return parent_; }
  std::string asOperand() const override;

private:
  Function *parent_;
};

class Function : public Value {
public:
  Function(std::string name, FunctionTypePtr type, bool declaration);

  std::string asOperand() const override;

  FunctionTypePtr getFunctionType() const { return funcType_; }

  Argument *addArgument(const TypePtr &type, const std::string &name);
  const std::vector<std::unique_ptr<Argument>> &getArguments() const;

  BasicBlock *createBasicBlock(const std::string &label);
  const std::vector<std::unique_ptr<BasicBlock>> &getBlocks() const {
    return blocks_;
  }

  bool isDeclaration() const { return isDeclaration_; }
  void setDeclaration(bool flag) { isDeclaration_ = flag; }

  std::string prototype() const;
  std::string print() const;

  std::string nextValueName(const std::string &hint);
  std::string nextBlockLabel(const std::string &hint);

private:
  FunctionTypePtr funcType_;
  std::vector<std::unique_ptr<Argument>> arguments_;
  std::vector<std::unique_ptr<BasicBlock>> blocks_;
  bool isDeclaration_;

  std::size_t valueId_;
  std::size_t blockId_;
};

} // namespace ir
