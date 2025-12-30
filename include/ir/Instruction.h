// IR 指令类型声明
#pragma once

#include "ir/Type.h"
#include "ir/Value.h"

#include <memory>
#include <string>
#include <vector>

namespace ir {

class BasicBlock;

enum class BinaryOp { Add, Sub, Mul, SDiv, SRem, And, Or, Xor }; // 少用的异或依然保留

enum class CmpOp { EQ, NE, LT, LE, GT, GE };

enum class InstructionKind {
  Alloca,
  Store,
  Load,
  Binary,
  ICmp,
  Call,
  Ret,
  Br,
  GEP,
  ZExt
};

class Instruction : public Value {
public:
  Instruction(TypePtr type, InstructionKind kind, bool hasResult);
  ~Instruction() override = default;

  InstructionKind getInstructionKind() const { return kind_; }
  BasicBlock *getParent() const { return parent_; }
  void setParent(BasicBlock *parent) { parent_ = parent; }

  bool hasResult() const { return hasResult_; }

  std::string asOperand() const override;
  virtual std::string toIR() const = 0;

protected:
  static std::string valueWithType(const Value *value);

  InstructionKind kind_;
  BasicBlock *parent_;
  bool hasResult_;
};

class AllocaInst : public Instruction {
public:
  explicit AllocaInst(const TypePtr &allocatedType);

  const TypePtr &getAllocatedType() const { return allocatedType_; }
  std::string toIR() const override;

private:
  TypePtr allocatedType_;
};

class StoreInst : public Instruction {
public:
  StoreInst(Value *value, Value *pointer);

  Value *getValue() const { return value_; }
  Value *getPointer() const { return pointer_; }
  std::string toIR() const override;

private:
  Value *value_;
  Value *pointer_;
};

class LoadInst : public Instruction {
public:
  LoadInst(const TypePtr &loadType, Value *pointer);

  Value *getPointer() const { return pointer_; }
  std::string toIR() const override;

private:
  Value *pointer_;
};

class BinaryInst : public Instruction {
public:
  BinaryInst(BinaryOp op, Value *lhs, Value *rhs);

  Value *getLHS() const { return lhs_; }
  Value *getRHS() const { return rhs_; }
  BinaryOp getOp() const { return op_; }
  void setOperands(Value *lhs, Value *rhs) {
    lhs_ = lhs;
    rhs_ = rhs;
  }
  std::string toIR() const override;

private:
  static std::string toOpcode(BinaryOp op);

  BinaryOp op_;
  Value *lhs_;
  Value *rhs_;
};

class ICmpInst : public Instruction {
public:
  ICmpInst(CmpOp predicate, Value *lhs, Value *rhs);

  Value *getLHS() const { return lhs_; }
  Value *getRHS() const { return rhs_; }
  CmpOp getPredicate() const { return predicate_; }
  void setOperands(Value *lhs, Value *rhs) {
    lhs_ = lhs;
    rhs_ = rhs;
  }
  std::string toIR() const override;

private:
  static std::string toPredicate(CmpOp predicate);

  CmpOp predicate_;
  Value *lhs_;
  Value *rhs_;
};

class CallInst : public Instruction {
public:
  CallInst(FunctionTypePtr funcType, Value *callee, std::vector<Value *> args);

  const std::vector<Value *> &getArgs() const { return args_; }
  Value *getCallee() const { return callee_; }
  std::string toIR() const override;

private:
  FunctionTypePtr funcType_;
  Value *callee_;
  std::vector<Value *> args_;
};

class ReturnInst : public Instruction {
public:
  explicit ReturnInst(Value *value = nullptr);

  Value *getValue() const { return value_; }
  void setValue(Value *value) { value_ = value; }
  std::string toIR() const override;

private:
  Value *value_;
};

class BranchInst : public Instruction {
public:
  BranchInst(BasicBlock *target);
  BranchInst(Value *condition, BasicBlock *trueBlock, BasicBlock *falseBlock);

  bool isConditional() const { return condition_ != nullptr; }
  Value *getCondition() const { return condition_; }
  void setCondition(Value *cond) { condition_ = cond; }
  BasicBlock *getTrueBlock() const { return trueBlock_; }
  BasicBlock *getFalseBlock() const { return falseBlock_; }
  std::string toIR() const override;

private:
  Value *condition_;
  BasicBlock *trueBlock_;
  BasicBlock *falseBlock_;
};

class GetElementPtrInst : public Instruction {
public:
  GetElementPtrInst(const TypePtr &elementType, Value *pointer,
                    std::vector<Value *> indices);

  Value *getPointer() const { return pointer_; }
  const std::vector<Value *> &getIndices() const { return indices_; }
  void setPointer(Value *ptr) { pointer_ = ptr; }
  void setIndex(std::size_t i, Value *v) { if (i < indices_.size()) indices_[i] = v; }
  std::string toIR() const override;

private:
  TypePtr elementType_;
  Value *pointer_;
  std::vector<Value *> indices_;
};

class ZExtInst : public Instruction {
public:
  ZExtInst(Value *value, const TypePtr &destType);

  Value *getValue() const { return value_; }
  void setValue(Value *value) { value_ = value; }
  std::string toIR() const override;

private:
  Value *value_;
};

} // namespace ir
