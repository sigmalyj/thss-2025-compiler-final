#include "ir/Function.h"

#include <sstream>
#include <utility>

namespace ir {

Argument::Argument(TypePtr type, std::string name, Function *parent)
    : Value(std::move(type), ValueKind::Argument, std::move(name)),
      parent_(parent) {}

std::string Argument::asOperand() const { return '%' + name_; }

Function::Function(std::string name, FunctionTypePtr type, bool declaration)
    : Value(std::move(type), ValueKind::Function, std::move(name)),
      funcType_(std::dynamic_pointer_cast<FunctionType>(getType())),
      isDeclaration_(declaration), valueId_(0), blockId_(0) {}

std::string Function::asOperand() const { return '@' + name_; }

Argument *Function::addArgument(const TypePtr &type, const std::string &name) {
  std::string argName = name.empty() ? nextValueName("arg") : name;
  auto arg = std::make_unique<Argument>(type, argName, this);
  Argument *raw = arg.get();
  arguments_.push_back(std::move(arg));
  return raw;
}

const std::vector<std::unique_ptr<Argument>> &Function::getArguments() const {
  return arguments_;
}

BasicBlock *Function::createBasicBlock(const std::string &label) {
  std::string finalLabel = label.empty() ? nextBlockLabel("bb") : label;
  auto block =
      std::make_unique<BasicBlock>(this, finalLabel);
  BasicBlock *raw = block.get();
  blocks_.push_back(std::move(block));
  return raw;
}

std::string Function::prototype() const {
  std::ostringstream oss;
  oss << funcType_->getReturnType()->str() << ' ' << asOperand() << '(';

  if (!arguments_.empty()) {
    for (std::size_t i = 0; i < arguments_.size(); ++i) {
      if (i != 0) {
        oss << ", ";
      }
      oss << arguments_[i]->getType()->str();
      if (arguments_[i]->hasName()) {
        oss << ' ' << arguments_[i]->asOperand();
      }
    }
  } else {
    const auto &params = funcType_->getParamTypes();
    for (std::size_t i = 0; i < params.size(); ++i) {
      if (i != 0) {
        oss << ", ";
      }
      oss << params[i]->str();
    }
  }

  if (funcType_->isVarArg()) {
    bool hasParamsPrinted = !arguments_.empty()
                                ? true
                                : !funcType_->getParamTypes().empty();
    if (hasParamsPrinted) {
      oss << ", ";
    }
    oss << "...";
  }

  oss << ')';
  return oss.str();
}

std::string Function::print() const {
  if (isDeclaration_) {
    return "declare " + prototype();
  }

  std::ostringstream oss;
  oss << "define dso_local " << prototype() << " {\n";
  for (const auto &block : blocks_) {
    oss << block->print();
  }
  oss << "}\n";
  return oss.str();
}

std::string Function::nextValueName(const std::string &hint) {
  return hint + std::to_string(valueId_++);
}

std::string Function::nextBlockLabel(const std::string &hint) {
  return hint + std::to_string(blockId_++);
}

} // namespace ir
