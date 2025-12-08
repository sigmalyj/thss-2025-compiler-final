// 基本块声明：保存指令序列
#pragma once

#include <memory>
#include <string>
#include <vector>

namespace ir {

class Function;
class Instruction;

class BasicBlock {
public:
  BasicBlock(Function *parent, std::string label);

  const std::string &getLabel() const { return label_; }
  Function *getParent() const { return parent_; }

  Instruction *appendInstruction(std::unique_ptr<Instruction> inst);
  Instruction *insertBeforeTerminator(std::unique_ptr<Instruction> inst);
  const std::vector<std::unique_ptr<Instruction>> &getInstructions() const {
    return instructions_;
  }

  bool empty() const { return instructions_.empty(); }
  Instruction *getTerminator() const;

  std::string print() const;

private:
  Function *parent_;
  std::string label_;
  std::vector<std::unique_ptr<Instruction>> instructions_;
};

} // namespace ir
