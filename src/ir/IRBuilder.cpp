// 提供便捷的 IR 指令构造接口
#include "ir/IRBuilder.h"

#include "ir/Constant.h"

#include <stdexcept>
#include <utility>

namespace ir {

IRBuilder::IRBuilder()
    : module_(nullptr), currentFunction_(nullptr), insertBlock_(nullptr) {}

void IRBuilder::setCurrentFunction(Function *function) {
  currentFunction_ = function;
}

void IRBuilder::setInsertPoint(BasicBlock *block) { insertBlock_ = block; }

Instruction *IRBuilder::insertInstruction(std::unique_ptr<Instruction> inst,
                                          const std::string &hint) {
  if (!insertBlock_) {
    throw std::runtime_error("Insert point is not set");
  }
  if (!currentFunction_) {
    throw std::runtime_error("Current function is not set");
  }
  if (inst->hasResult() && !inst->hasName()) {
    inst->setName(currentFunction_->nextValueName(hint));
  }
  return insertBlock_->appendInstruction(std::move(inst));
}

AllocaInst *IRBuilder::createAlloca(const TypePtr &type,
                                    const std::string &hint) {
  auto inst = std::make_unique<AllocaInst>(type);
  return static_cast<AllocaInst *>(insertInstruction(std::move(inst), hint));
}

AllocaInst *IRBuilder::createAllocaAtEntry(const TypePtr &type,
                                           const std::string &hint) {
  if (!currentFunction_) {
    throw std::runtime_error("Current function is not set");
  }
  const auto &blocks = currentFunction_->getBlocks();
  if (blocks.empty()) {
    throw std::runtime_error("Function has no basic blocks");
  }

  auto *originalBlock = insertBlock_;
  auto *entry = blocks.front().get();
  auto inst = std::make_unique<AllocaInst>(type);

  inst->setName(currentFunction_->nextValueName(hint));
  inst->setParent(entry);

  Instruction *raw = nullptr;
  if (entry->getTerminator()) {
    raw = entry->insertBeforeTerminator(std::move(inst));
  } else {
    raw = entry->appendInstruction(std::move(inst));
  }

  insertBlock_ = originalBlock;
  return static_cast<AllocaInst *>(raw);
}

StoreInst *IRBuilder::createStore(Value *value, Value *pointer) {
  auto inst = std::make_unique<StoreInst>(value, pointer);
  return static_cast<StoreInst *>(insertInstruction(std::move(inst), "store"));
}

LoadInst *IRBuilder::createLoad(const TypePtr &type, Value *pointer,
                                const std::string &hint) {
  auto inst = std::make_unique<LoadInst>(type, pointer);
  return static_cast<LoadInst *>(insertInstruction(std::move(inst), hint));
}

BinaryInst *IRBuilder::createBinary(BinaryOp op, Value *lhs, Value *rhs,
                                    const std::string &hint) {
  auto inst = std::make_unique<BinaryInst>(op, lhs, rhs);
  return static_cast<BinaryInst *>(insertInstruction(std::move(inst), hint));
}

ICmpInst *IRBuilder::createICmp(CmpOp predicate, Value *lhs, Value *rhs,
                                const std::string &hint) {
  auto inst = std::make_unique<ICmpInst>(predicate, lhs, rhs);
  return static_cast<ICmpInst *>(insertInstruction(std::move(inst), hint));
}

CallInst *IRBuilder::createCall(Function *callee,
                                const std::vector<Value *> &args,
                                const std::string &hint) {
  auto inst =
      std::make_unique<CallInst>(callee->getFunctionType(), callee, args);
  return static_cast<CallInst *>(insertInstruction(std::move(inst), hint));
}

ReturnInst *IRBuilder::createRet(Value *value) {
  auto inst = std::make_unique<ReturnInst>(value);
  return static_cast<ReturnInst *>(insertInstruction(std::move(inst), "ret"));
}

BranchInst *IRBuilder::createBr(BasicBlock *target) {
  auto inst = std::make_unique<BranchInst>(target);
  return static_cast<BranchInst *>(insertInstruction(std::move(inst), "br"));
}

BranchInst *IRBuilder::createCondBr(Value *condition, BasicBlock *trueBlock,
                                    BasicBlock *falseBlock) {
  auto inst = std::make_unique<BranchInst>(condition, trueBlock, falseBlock);
  return static_cast<BranchInst *>(insertInstruction(std::move(inst), "br"));
}

GetElementPtrInst *IRBuilder::createGEP(const TypePtr &elementType,
                                        Value *pointer,
                                        const std::vector<Value *> &indices,
                                        const std::string &hint) {
  auto inst =
      std::make_unique<GetElementPtrInst>(elementType, pointer, indices);
  return static_cast<GetElementPtrInst *>(
      insertInstruction(std::move(inst), hint));
}

ZExtInst *IRBuilder::createZExt(Value *value, const TypePtr &destType,
                                const std::string &hint) {
  auto inst = std::make_unique<ZExtInst>(value, destType);
  return static_cast<ZExtInst *>(insertInstruction(std::move(inst), hint));
}

std::shared_ptr<ConstantInt> IRBuilder::getInt32(int value) const {
  return ConstantInt::get(value);
}

} // namespace ir
