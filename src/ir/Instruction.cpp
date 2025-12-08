// IR 指令节点的打印与辅助逻辑
#include "ir/Instruction.h"

#include "ir/BasicBlock.h"
#include "ir/Constant.h"

#include <sstream>
#include <utility>

namespace ir {

Instruction::Instruction(TypePtr type, InstructionKind kind, bool hasResult)
    : Value(std::move(type), ValueKind::Instruction), kind_(kind),
      parent_(nullptr), hasResult_(hasResult) {}

std::string Instruction::asOperand() const { return '%' + name_; }

std::string Instruction::valueWithType(const Value *value) {
  std::ostringstream oss;
  oss << value->getType()->str() << ' ' << value->asOperand();
  return oss.str();
}

AllocaInst::AllocaInst(const TypePtr &allocatedType)
    : Instruction(Type::getPointer(allocatedType), InstructionKind::Alloca, true),
      allocatedType_(allocatedType) {}

std::string AllocaInst::toIR() const {
  std::ostringstream oss;
  oss << asOperand() << " = alloca " << allocatedType_->str();
  return oss.str();
}

StoreInst::StoreInst(Value *value, Value *pointer)
    : Instruction(Type::getVoid(), InstructionKind::Store, false), value_(value),
      pointer_(pointer) {}

std::string StoreInst::toIR() const {
  std::ostringstream oss;
  oss << "store " << valueWithType(value_) << ", "
      << pointer_->getType()->str() << ' ' << pointer_->asOperand();
  return oss.str();
}

LoadInst::LoadInst(const TypePtr &loadType, Value *pointer)
  : Instruction(loadType, InstructionKind::Load, true), pointer_(pointer) {}

std::string LoadInst::toIR() const {
  std::ostringstream oss;
  oss << asOperand() << " = load " << getType()->str() << ", "
      << pointer_->getType()->str() << ' ' << pointer_->asOperand();
  return oss.str();
}

BinaryInst::BinaryInst(BinaryOp op, Value *lhs, Value *rhs)
    : Instruction(Type::getInt32(), InstructionKind::Binary, true), op_(op),
      lhs_(lhs), rhs_(rhs) {}

std::string BinaryInst::toIR() const {
  std::ostringstream oss;
  oss << asOperand() << ' ' << "= " << toOpcode(op_) << ' '
      << lhs_->getType()->str() << ' ' << lhs_->asOperand() << ", "
      << rhs_->asOperand();
  return oss.str();
}

std::string BinaryInst::toOpcode(BinaryOp op) {
  switch (op) {
  case BinaryOp::Add:
    return "add";
  case BinaryOp::Sub:
    return "sub";
  case BinaryOp::Mul:
    return "mul";
  case BinaryOp::SDiv:
    return "sdiv";
  case BinaryOp::SRem:
    return "srem";
  case BinaryOp::And:
    return "and";
  case BinaryOp::Or:
    return "or";
  case BinaryOp::Xor:
    return "xor";
  }
  return "add";
}

ICmpInst::ICmpInst(CmpOp predicate, Value *lhs, Value *rhs)
    : Instruction(Type::getInt1(), InstructionKind::ICmp, true),
      predicate_(predicate), lhs_(lhs), rhs_(rhs) {}

std::string ICmpInst::toIR() const {
  std::ostringstream oss;
  oss << asOperand() << " = icmp " << toPredicate(predicate_) << ' '
      << lhs_->getType()->str() << ' ' << lhs_->asOperand() << ", "
      << rhs_->asOperand();
  return oss.str();
}

std::string ICmpInst::toPredicate(CmpOp predicate) {
  switch (predicate) {
  case CmpOp::EQ:
    return "eq";
  case CmpOp::NE:
    return "ne";
  case CmpOp::LT:
    return "slt";
  case CmpOp::LE:
    return "sle";
  case CmpOp::GT:
    return "sgt";
  case CmpOp::GE:
    return "sge";
  }
  return "eq";
}

CallInst::CallInst(FunctionTypePtr funcType, Value *callee, std::vector<Value *> args)
    : Instruction(funcType->getReturnType()->isVoid() ? Type::getVoid()
                                                      : funcType->getReturnType(),
                  InstructionKind::Call,
                  !funcType->getReturnType()->isVoid()),
      funcType_(std::move(funcType)), callee_(callee), args_(std::move(args)) {}

std::string CallInst::toIR() const {
  std::ostringstream oss;
  if (hasResult()) {
    oss << asOperand() << " = ";
  }
  oss << "call " << funcType_->getReturnType()->str() << ' '
      << callee_->asOperand() << '(';
  for (std::size_t i = 0; i < args_.size(); ++i) {
    if (i != 0) {
      oss << ", ";
    }
    oss << args_[i]->getType()->str() << ' ' << args_[i]->asOperand();
  }
  oss << ')';
  return oss.str();
}

ReturnInst::ReturnInst(Value *value)
    : Instruction(value ? value->getType() : Type::getVoid(), InstructionKind::Ret,
                  false),
      value_(value) {}

std::string ReturnInst::toIR() const {
  std::ostringstream oss;
  if (value_) {
    oss << "ret " << value_->getType()->str() << ' ' << value_->asOperand();
  } else {
    oss << "ret void";
  }
  return oss.str();
}

BranchInst::BranchInst(BasicBlock *target)
    : Instruction(Type::getVoid(), InstructionKind::Br, false),
      condition_(nullptr), trueBlock_(target), falseBlock_(nullptr) {}

BranchInst::BranchInst(Value *condition, BasicBlock *trueBlock,
                       BasicBlock *falseBlock)
    : Instruction(Type::getVoid(), InstructionKind::Br, false),
      condition_(condition), trueBlock_(trueBlock), falseBlock_(falseBlock) {}

std::string BranchInst::toIR() const {
  std::ostringstream oss;
  if (!condition_) {
    oss << "br label %" << trueBlock_->getLabel();
  } else {
    oss << "br i1 " << condition_->asOperand() << ", label %"
        << trueBlock_->getLabel() << ", label %" << falseBlock_->getLabel();
  }
  return oss.str();
}

GetElementPtrInst::GetElementPtrInst(const TypePtr &elementType, Value *pointer,
                                     std::vector<Value *> indices)
    : Instruction(Type::getPointer(elementType), InstructionKind::GEP, true),
      elementType_(elementType), pointer_(pointer), indices_(std::move(indices)) {
  TypePtr currentType = elementType;
  for (size_t i = 1; i < indices_.size(); ++i) {
    if (currentType->isArray()) {
      currentType = std::static_pointer_cast<ArrayType>(currentType)->getElementType();
    }
  }
  type_ = Type::getPointer(currentType);
}

std::string GetElementPtrInst::toIR() const {
  std::ostringstream oss;
  oss << asOperand() << " = getelementptr inbounds "
      << elementType_->str() << ", " << pointer_->getType()->str()
      << ' ' << pointer_->asOperand();  
  for (Value *idx : indices_) {
    oss << ", i32 " << idx->asOperand();
  }
  return oss.str();
}

ZExtInst::ZExtInst(Value *value, const TypePtr &destType)
    : Instruction(destType, InstructionKind::ZExt, true), value_(value) {}

std::string ZExtInst::toIR() const {
  std::ostringstream oss;
  oss << asOperand() << " = zext " << value_->getType()->str() << ' '
      << value_->asOperand() << " to " << getType()->str();
  return oss.str();
}

} // namespace ir
