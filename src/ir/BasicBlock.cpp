// 基本块的指令存储与打印
#include "ir/BasicBlock.h"

#include "ir/Instruction.h"

#include <sstream>
#include <utility>

namespace ir {

BasicBlock::BasicBlock(Function *parent, std::string label)
    : parent_(parent), label_(std::move(label)) {}

Instruction *BasicBlock::appendInstruction(std::unique_ptr<Instruction> inst) {
  inst->setParent(this);
  Instruction *raw = inst.get();
  instructions_.push_back(std::move(inst));
  return raw;
}

Instruction *BasicBlock::insertBeforeTerminator(
    std::unique_ptr<Instruction> inst) {
  inst->setParent(this);
  Instruction *raw = inst.get();

  if (instructions_.empty()) {
    instructions_.push_back(std::move(inst));
    return raw;
  }

  auto *term = getTerminator();
  if (!term) {
    instructions_.push_back(std::move(inst));
    return raw;
  }

  auto it = instructions_.end();
  --it; // Point to terminator position.
  instructions_.insert(it, std::move(inst));
  return raw;
}

Instruction *BasicBlock::getTerminator() const {
  if (instructions_.empty()) {
    return nullptr;
  }
  auto kind = instructions_.back()->getInstructionKind();
  if (kind == InstructionKind::Br || kind == InstructionKind::Ret) {
    return instructions_.back().get();
  }
  return nullptr;
}

std::string BasicBlock::print() const {
  std::ostringstream oss;
  oss << label_ << ":\n";
  for (const auto &inst : instructions_) {
    oss << "  " << inst->toIR() << '\n';
  }
  oss << '\n';
  return oss.str();
}

} // namespace ir
