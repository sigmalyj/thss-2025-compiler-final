#pragma once

#include "ir/Instruction.h"
#include "ir/Module.h"

#include <memory>
#include <string>
#include <vector>

namespace ir {

class IRBuilder {
public:
  IRBuilder();

  void setModule(Module *module) { module_ = module; }
  Module *getModule() const { return module_; }

  void setCurrentFunction(Function *function);
  Function *getCurrentFunction() const { return currentFunction_; }

  void setInsertPoint(BasicBlock *block);
  BasicBlock *getInsertBlock() const { return insertBlock_; }

  AllocaInst *createAlloca(const TypePtr &type, const std::string &hint = "tmp");
    AllocaInst *createAllocaAtEntry(const TypePtr &type,
                                    const std::string &hint = "tmp");
  StoreInst *createStore(Value *value, Value *pointer);
  LoadInst *createLoad(const TypePtr &type, Value *pointer,
                       const std::string &hint = "tmp");
  BinaryInst *createBinary(BinaryOp op, Value *lhs, Value *rhs,
                           const std::string &hint = "tmp");
  ICmpInst *createICmp(CmpOp predicate, Value *lhs, Value *rhs,
                       const std::string &hint = "cmp");
  CallInst *createCall(Function *callee, const std::vector<Value *> &args,
                       const std::string &hint = "call");
  ReturnInst *createRet(Value *value = nullptr);
  BranchInst *createBr(BasicBlock *target);
  BranchInst *createCondBr(Value *condition, BasicBlock *trueBlock,
                           BasicBlock *falseBlock);
  GetElementPtrInst *createGEP(const TypePtr &elementType, Value *pointer,
                               const std::vector<Value *> &indices,
                               const std::string &hint = "gep");
  ZExtInst *createZExt(Value *value, const TypePtr &destType,
                       const std::string &hint = "zext");

  std::shared_ptr<ConstantInt> getInt32(int value) const;

private:
  Instruction *insertInstruction(std::unique_ptr<Instruction> inst,
                                 const std::string &hint);

  Module *module_;
  Function *currentFunction_;
  BasicBlock *insertBlock_;
};

} // namespace ir
