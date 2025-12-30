// 简单机器无关优化：前向常量传播 + 无用代码删除
#include "compiler/Optimizer.h"

#include "ir/BasicBlock.h"
#include "ir/Instruction.h"
#include "ir/Constant.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace compiler {
namespace {

using ConstMap = std::unordered_map<ir::Value *, std::shared_ptr<ir::ConstantInt>>;
using UseCount = std::unordered_map<ir::Value *, int>;

bool isConstInt(ir::Value *v, int &out) {
  if (auto *c = dynamic_cast<ir::ConstantInt *>(v)) {
    out = c->getValue();
    return true;
  }
  return false;
}

std::vector<ir::Value *> collectOperands(ir::Instruction *inst) {
  using K = ir::InstructionKind;
  std::vector<ir::Value *> ops;
  switch (inst->getInstructionKind()) {
  case K::Store: {
    auto *s = static_cast<ir::StoreInst *>(inst);
    ops.push_back(s->getValue());
    ops.push_back(s->getPointer());
    break;
  }
  case K::Load: {
    auto *l = static_cast<ir::LoadInst *>(inst);
    ops.push_back(l->getPointer());
    break;
  }
  case K::Binary: {
    auto *b = static_cast<ir::BinaryInst *>(inst);
    ops.push_back(b->getLHS());
    ops.push_back(b->getRHS());
    break;
  }
  case K::ICmp: {
    auto *c = static_cast<ir::ICmpInst *>(inst);
    ops.push_back(c->getLHS());
    ops.push_back(c->getRHS());
    break;
  }
  case K::Call: {
    auto *c = static_cast<ir::CallInst *>(inst);
    ops.push_back(c->getCallee());
    for (auto *arg : c->getArgs()) ops.push_back(arg);
    break;
  }
  case K::Ret: {
    auto *r = static_cast<ir::ReturnInst *>(inst);
    if (auto *v = r->getValue()) ops.push_back(v);
    break;
  }
  case K::Br: {
    auto *b = static_cast<ir::BranchInst *>(inst);
    if (b->isConditional()) ops.push_back(b->getCondition());
    break;
  }
  case K::GEP: {
    auto *g = static_cast<ir::GetElementPtrInst *>(inst);
    ops.push_back(g->getPointer());
    for (auto *idx : g->getIndices()) ops.push_back(idx);
    break;
  }
  case K::ZExt: {
    auto *z = static_cast<ir::ZExtInst *>(inst);
    ops.push_back(z->getValue());
    break;
  }
  case K::Alloca:
  default:
    break;
  }
  return ops;
}

bool isSideEffectFree(const ir::Instruction *inst) {
  using K = ir::InstructionKind;
  switch (inst->getInstructionKind()) {
  case K::Binary:
  case K::ICmp:
  case K::Alloca:
  case K::Load:
  case K::GEP:
  case K::ZExt:
    return true;
  default:
    return false;
  }
}

void forwardConstPropagation(ir::Function *func) {
  ConstMap constMap;
  for (const auto &blockPtr : func->getBlocks()) {
    auto *block = blockPtr.get();
    for (auto &instPtr : block->instructions()) {
      auto *inst = instPtr.get();

      // 先用 constMap 替换操作数
      switch (inst->getInstructionKind()) {
      case ir::InstructionKind::Binary: {
        auto *b = static_cast<ir::BinaryInst *>(inst);
        auto *lhs = b->getLHS();
        auto *rhs = b->getRHS();
        if (auto it = constMap.find(lhs); it != constMap.end()) lhs = it->second.get();
        if (auto it = constMap.find(rhs); it != constMap.end()) rhs = it->second.get();
        b->setOperands(lhs, rhs);
        int lv = 0, rv = 0;
        if (isConstInt(lhs, lv) && isConstInt(rhs, rv)) {
          int res = 0;
          switch (b->getOp()) {
          case ir::BinaryOp::Add: res = lv + rv; break;
          case ir::BinaryOp::Sub: res = lv - rv; break;
          case ir::BinaryOp::Mul: res = lv * rv; break;
          case ir::BinaryOp::SDiv: res = rv != 0 ? lv / rv : lv; break;
          case ir::BinaryOp::SRem: res = rv != 0 ? lv % rv : lv; break;
          case ir::BinaryOp::And: res = lv & rv; break;
          case ir::BinaryOp::Or:  res = lv | rv; break;
          case ir::BinaryOp::Xor: res = lv ^ rv; break;
          }
          constMap[inst] = ir::ConstantInt::get(res);
        }
        break;
      }
      case ir::InstructionKind::ICmp: {
        auto *c = static_cast<ir::ICmpInst *>(inst);
        auto *lhs = c->getLHS();
        auto *rhs = c->getRHS();
        if (auto it = constMap.find(lhs); it != constMap.end()) lhs = it->second.get();
        if (auto it = constMap.find(rhs); it != constMap.end()) rhs = it->second.get();
        c->setOperands(lhs, rhs);
        int lv = 0, rv = 0;
        if (isConstInt(lhs, lv) && isConstInt(rhs, rv)) {
          bool res = false;
          switch (c->getPredicate()) {
          case ir::CmpOp::EQ: res = lv == rv; break;
          case ir::CmpOp::NE: res = lv != rv; break;
          case ir::CmpOp::LT: res = lv < rv; break;
          case ir::CmpOp::LE: res = lv <= rv; break;
          case ir::CmpOp::GT: res = lv > rv; break;
          case ir::CmpOp::GE: res = lv >= rv; break;
          }
          constMap[inst] = ir::ConstantInt::get(res ? 1 : 0);
        }
        break;
      }
      case ir::InstructionKind::ZExt: {
        auto *z = static_cast<ir::ZExtInst *>(inst);
        auto *v = z->getValue();
        if (auto it = constMap.find(v); it != constMap.end()) v = it->second.get();
        z->setValue(v);
        int val = 0;
        if (isConstInt(v, val)) {
          constMap[inst] = ir::ConstantInt::get(val);
        }
        break;
      }
      case ir::InstructionKind::GEP: {
        auto *g = static_cast<ir::GetElementPtrInst *>(inst);
        auto *ptr = g->getPointer();
        if (auto it = constMap.find(ptr); it != constMap.end()) ptr = it->second.get();
        g->setPointer(ptr);
        for (std::size_t i = 0; i < g->getIndices().size(); ++i) {
          auto *idx = g->getIndices()[i];
          if (auto it = constMap.find(idx); it != constMap.end()) {
            g->setIndex(i, it->second.get());
          }
        }
        break;
      }
      case ir::InstructionKind::Br: {
        auto *b = static_cast<ir::BranchInst *>(inst);
        if (b->isConditional()) {
          auto *cond = b->getCondition();
          if (auto it = constMap.find(cond); it != constMap.end()) {
            b->setCondition(it->second.get());
          }
        }
        break;
      }
      case ir::InstructionKind::Ret: {
        auto *r = static_cast<ir::ReturnInst *>(inst);
        if (auto *v = r->getValue()) {
          if (auto it = constMap.find(v); it != constMap.end()) {
            r->setValue(it->second.get());
          }
        }
        break;
      }
      case ir::InstructionKind::Store: {
        // store 无结果也不做常量传播
        break;
      }
      case ir::InstructionKind::Load: {
        // load 保持原状
        break;
      }
      default:
        break;
      }
    }
  }
}

void deadCodeElimination(ir::Function *func) {
  bool changed = true;
  while (changed) {
    changed = false;
    UseCount uses;

    // 统计使用次数
    for (const auto &blockPtr : func->getBlocks()) {
      for (const auto &instPtr : blockPtr->getInstructions()) {
        auto *inst = instPtr.get();
        for (auto *op : collectOperands(inst)) {
          uses[op]++;
        }
      }
    }

    // 执行删除
    for (const auto &blockPtr : func->getBlocks()) {
      auto *block = blockPtr.get();
      std::vector<std::unique_ptr<ir::Instruction>> kept;
      for (auto &instPtr : block->instructions()) {
        auto *inst = instPtr.get();
        bool removable = !inst->hasResult() ? false : isSideEffectFree(inst);
        int useCount = uses.count(inst) ? uses[inst] : 0;
        if (removable && useCount == 0) {
          // 减少其操作数的使用计数
          for (auto *op : collectOperands(inst)) {
            if (uses[op] > 0) {
              uses[op]--;
            }
          }
          changed = true;
          continue;
        }
        kept.push_back(std::move(instPtr));
      }
      block->instructions().swap(kept);
    }
  }
}

void optimizeFunction(ir::Function *func) {
  forwardConstPropagation(func);
  deadCodeElimination(func);
}

} // namespace

void runOptimizations(ir::Module &module) {
  for (const auto &funcPtr : module.getFunctions()) {
    if (funcPtr->isDeclaration()) continue;
    optimizeFunction(funcPtr.get());
  }
}

} // namespace compiler
