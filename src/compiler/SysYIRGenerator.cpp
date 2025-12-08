#include "compiler/SysYIRGenerator.h"

#include "SysYParserBaseVisitor.h"
#include "antlr4-runtime.h"
#include "ir/Constant.h"

#include <algorithm>
#include <numeric>
#include <stdexcept>

namespace compiler {

namespace {

int parseIntegerLiteral(const std::string &text) {
  if (text.size() > 2 && (text[0] == '0') && (text[1] == 'x' || text[1] == 'X')) {
    return std::stoi(text, nullptr, 16);
  }
  if (text.size() > 1 && text[0] == '0') {
    return std::stoi(text, nullptr, 8);
  }
  return std::stoi(text, nullptr, 10);
}

} // namespace

SysYIRGenerator::SysYIRGenerator(ir::Module &module, ir::IRBuilder &builder)
    : module_(module), builder_(builder) {
  builder_.setModule(&module_);
}

bool SysYIRGenerator::isGlobalScope() const {
  return builder_.getCurrentFunction() == nullptr;
}

void SysYIRGenerator::generate(SysYParser::CompUnitContext *ctx) {
  for (auto *child : ctx->children) {
    if (auto *decl = dynamic_cast<SysYParser::DeclContext *>(child)) {
      handleDecl(decl);
    } else if (auto *func = dynamic_cast<SysYParser::FuncDefContext *>(child)) {
      auto funcTypeToken = func->funcType()->getText();
      auto functionName = func->IDENT()->getText();
      ir::TypePtr returnType =
          (funcTypeToken == "int") ? ir::Type::getInt32() : ir::Type::getVoid();

      std::vector<ir::TypePtr> paramTypes;
      std::vector<std::string> paramNames;
      std::vector<std::vector<int>> paramDims;
      std::vector<bool> pointerParams;

      if (auto *params = func->funcFParams()) {
        for (auto *param : params->funcFParam()) {
          auto name = param->IDENT()->getText();
          std::vector<int> dims;
          bool isPointerParam = false;
          if (!param->LBRACK().empty()) {
            isPointerParam = true;
            auto indices = param->constExp();
            if (!indices.empty()) {
              auto subDims = collectDimensions(indices);
              dims = subDims;
            }
          }
          ir::TypePtr paramType = ir::Type::getInt32();
          if (!dims.empty()) {
            paramType = buildArrayType(dims);
          }
          if (isPointerParam) {
            paramType = ir::Type::getPointer(paramType);
          }
          paramTypes.push_back(paramType);
          paramNames.push_back(name);
          paramDims.push_back(dims);
          pointerParams.push_back(isPointerParam);
        }
      }

      auto functionType = ir::Type::getFunction(returnType, paramTypes);
      auto *function = module_.createFunction(functionName, functionType, false);
      builder_.setCurrentFunction(function);
      symbols_.enterScope();

      auto *entry = function->createBasicBlock("entry");
      builder_.setInsertPoint(entry);

      for (std::size_t i = 0; i < paramTypes.size(); ++i) {
        auto *arg = function->addArgument(paramTypes[i], paramNames[i]);
        Symbol symbol;
        symbol.valueType = ir::Type::getInt32();
        symbol.isConst = false;
        symbol.isGlobal = false;
        symbol.isPointerParam = pointerParams[i];
        symbol.dimensions = paramDims[i];
        if (!pointerParams[i]) {
          auto *alloca = builder_.createAlloca(ir::Type::getInt32(), paramNames[i] + ".addr");
          builder_.createStore(arg, alloca);
          symbol.address = alloca;
        } else {
          symbol.address = arg;
        }
        symbols_.insert(paramNames[i], symbol);
      }

      emitBlock(func->block());

      if (!isCurrentBlockTerminated()) {
        if (returnType->isVoid()) {
          builder_.createRet(nullptr);
        } else {
          builder_.createRet(builder_.getInt32(0).get());
        }
      }

      symbols_.exitScope();
      builder_.setCurrentFunction(nullptr);
      builder_.setInsertPoint(nullptr);
    }
  }
}

void SysYIRGenerator::handleDecl(SysYParser::DeclContext *ctx) {
  if (auto *constDecl = ctx->constDecl()) {
    handleConstDecl(constDecl);
  } else if (auto *varDecl = ctx->varDecl()) {
    handleVarDecl(varDecl);
  }
}

void SysYIRGenerator::handleConstDecl(SysYParser::ConstDeclContext *ctx) {
  for (auto *def : ctx->constDef()) {
    handleConstDef(def);
  }
}

void SysYIRGenerator::handleVarDecl(SysYParser::VarDeclContext *ctx) {
  for (auto *def : ctx->varDef()) {
    handleVarDef(def);
  }
}

std::vector<int> SysYIRGenerator::collectDimensions(
    const std::vector<SysYParser::ConstExpContext *> &contexts) {
  std::vector<int> dims;
  dims.reserve(contexts.size());
  for (auto *exp : contexts) {
    dims.push_back(evalConstExp(exp));
  }
  return dims;
}

std::size_t SysYIRGenerator::totalSize(const std::vector<int> &dims) const {
  if (dims.empty()) {
    return 1;
  }
  return std::accumulate(dims.begin(), dims.end(), static_cast<std::size_t>(1),
                         [](std::size_t acc, int dim) {
                           return acc * static_cast<std::size_t>(dim);
                         });
}

ir::TypePtr SysYIRGenerator::buildArrayType(const std::vector<int> &dims) const {
  ir::TypePtr type = ir::Type::getInt32();
  for (auto it = dims.rbegin(); it != dims.rend(); ++it) {
    type = ir::Type::getArray(type, static_cast<std::size_t>(*it));
  }
  return type;
}

void SysYIRGenerator::handleConstDef(SysYParser::ConstDefContext *ctx) {
  auto name = ctx->IDENT()->getText();
  auto dims = collectDimensions(ctx->constExp());
  auto total = totalSize(dims);

  ConstValueList values;
  if (ctx->constInitVal()) {
    values = materializeConstInit(ctx->constInitVal(), dims);
  } else {
    values.assign(total, 0);
  }

  Symbol symbol;
  symbol.valueType = ir::Type::getInt32();
  symbol.isConst = true;
  symbol.dimensions = dims;
  symbol.isGlobal = isGlobalScope();

  assignConstData(symbol, values);

  if (isGlobalScope()) {
    ir::TypePtr valueType = dims.empty() ? ir::Type::getInt32() : buildArrayType(dims);
    std::shared_ptr<ir::Constant> initializer;
    if (dims.empty()) {
      initializer = ir::ConstantInt::get(values.empty() ? 0 : values[0]);
    } else {
      std::size_t cursor = 0;
      initializer = buildArrayConstant(valueType, symbol.constData, cursor);
    }
    auto *global = module_.addGlobal(name, valueType, true, initializer);
    symbol.address = global;
  } else {
    if (dims.empty()) {
      auto *alloca = builder_.createAlloca(ir::Type::getInt32(), name + ".addr");
      auto *value = builder_.getInt32(values.empty() ? 0 : values[0]).get();
      builder_.createStore(value, alloca);
      symbol.address = alloca;
    } else {
      auto arrayType = buildArrayType(dims);
      auto *alloca = builder_.createAlloca(arrayType, name + ".addr");
      symbol.address = alloca;
      for (std::size_t i = 0; i < total; ++i) {
        auto *value = builder_.getInt32(symbol.constData[i]).get();
        storeLinearElement(alloca, dims, i, value);
      }
    }
  }

  if (!symbols_.insert(name, symbol)) {
    throw std::runtime_error("duplicate symbol: " + name);
  }
}

void SysYIRGenerator::handleVarDef(SysYParser::VarDefContext *ctx) {
  auto name = ctx->IDENT()->getText();
  auto dims = collectDimensions(ctx->constExp());
  auto total = totalSize(dims);

  Symbol symbol;
  symbol.valueType = ir::Type::getInt32();
  symbol.isConst = false;
  symbol.dimensions = dims;
  symbol.isGlobal = isGlobalScope();

  if (isGlobalScope()) {
    ir::TypePtr valueType = dims.empty() ? ir::Type::getInt32() : buildArrayType(dims);
    std::shared_ptr<ir::Constant> initializer;
    if (ctx->initVal()) {
      ConstValueList values = materializeInit(ctx->initVal(), dims);
      std::size_t cursor = 0;
      initializer = dims.empty() ? ir::ConstantInt::get(values[0])
                                 : buildArrayConstant(valueType, values, cursor);
      assignConstData(symbol, values);
    }
    auto *global = module_.addGlobal(name, valueType, false, initializer);
    symbol.address = global;
  } else {
    if (dims.empty()) {
      auto *alloca = builder_.createAlloca(ir::Type::getInt32(), name + ".addr");
      symbol.address = alloca;
      if (ctx->initVal()) {
        auto *value = ensureInteger(evaluateExp(ctx->initVal()->exp()));
        builder_.createStore(value, alloca);
      } else {
        builder_.createStore(builder_.getInt32(0).get(), alloca);
      }
    } else {
      auto arrayType = buildArrayType(dims);
      auto *alloca = builder_.createAlloca(arrayType, name + ".addr");
      symbol.address = alloca;
      if (ctx->initVal()) {
        emitArrayInitializer(alloca, dims, ctx->initVal());
      }
    }
  }

  if (!symbols_.insert(name, symbol)) {
    throw std::runtime_error("duplicate symbol: " + name);
  }
}

void SysYIRGenerator::emitBlock(SysYParser::BlockContext *ctx) {
  symbols_.enterScope();
  for (auto *item : ctx->blockItem()) {
    if (item->decl()) {
      handleDecl(item->decl());
    } else if (item->stmt()) {
      emitStmt(item->stmt());
    }
    if (isCurrentBlockTerminated()) {
      break;
    }
  }
  symbols_.exitScope();
}

bool SysYIRGenerator::isCurrentBlockTerminated() const {
  auto *block = builder_.getInsertBlock();
  return block && block->getTerminator() != nullptr;
}

void SysYIRGenerator::emitStmt(SysYParser::StmtContext *ctx) {
  if (isCurrentBlockTerminated()) {
    return;
  }

  if (ctx->lVal()) {
    auto *lval = getLValAddress(ctx->lVal());
    auto *rhs = evaluateExp(ctx->exp());
    rhs = ensureInteger(rhs);
    builder_.createStore(rhs, lval);
  } else if (ctx->block()) {
    emitBlock(ctx->block());
  } else if (ctx->IF()) {
    auto *cond = ctx->cond();
    auto *thenStmt = ctx->stmt(0);
    auto *elseStmt = ctx->ELSE() ? ctx->stmt(1) : nullptr;

    auto *func = builder_.getCurrentFunction();
    auto *thenBlock = func->createBasicBlock("if.then");
    auto *elseBlock = elseStmt ? func->createBasicBlock("if.else") : nullptr;
    auto *mergeBlock = func->createBasicBlock("if.end");

    auto *falseTarget = elseStmt ? elseBlock : mergeBlock;

    emitCondition(cond, thenBlock, falseTarget);

    builder_.setInsertPoint(thenBlock);
    emitStmt(thenStmt);
    if (!isCurrentBlockTerminated()) {
      builder_.createBr(mergeBlock);
    }

    if (elseStmt) {
      builder_.setInsertPoint(elseBlock);
      emitStmt(elseStmt);
      if (!isCurrentBlockTerminated()) {
        builder_.createBr(mergeBlock);
      }
    }

    builder_.setInsertPoint(mergeBlock);
  } else if (ctx->WHILE()) {
    auto *func = builder_.getCurrentFunction();
    auto *condBlock = func->createBasicBlock("while.cond");
    auto *bodyBlock = func->createBasicBlock("while.body");
    auto *endBlock = func->createBasicBlock("while.end");

    builder_.createBr(condBlock);

    builder_.setInsertPoint(condBlock);
    emitCondition(ctx->cond(), bodyBlock, endBlock);

    builder_.setInsertPoint(bodyBlock);
    loopStack_.emplace_back(condBlock, endBlock);
    emitStmt(ctx->stmt(0));
    loopStack_.pop_back();

    if (!isCurrentBlockTerminated()) {
      builder_.createBr(condBlock);
    }

    builder_.setInsertPoint(endBlock);
  } else if (ctx->BREAK()) {
    if (loopStack_.empty()) {
      throw std::runtime_error("break statement not within loop");
    }
    builder_.createBr(loopStack_.back().second);
  } else if (ctx->CONTINUE()) {
    if (loopStack_.empty()) {
      throw std::runtime_error("continue statement not within loop");
    }
    builder_.createBr(loopStack_.back().first);
  } else if (ctx->RETURN()) {
    if (ctx->exp()) {
      auto *val = evaluateExp(ctx->exp());
      val = ensureInteger(val);
      builder_.createRet(val);
    } else {
      builder_.createRet(nullptr);
    }
  } else if (ctx->exp()) {
    evaluateExp(ctx->exp());
  } else {
    // Empty statement
  }
}

// ... (Remaining member function implementations will follow here)

void SysYIRGenerator::emitCondition(SysYParser::CondContext *ctx,
                                    ir::BasicBlock *trueBlock,
                                    ir::BasicBlock *falseBlock) {
  emitLor(ctx->lorExp(), trueBlock, falseBlock);
}

void SysYIRGenerator::emitLor(SysYParser::LorExpContext *ctx,
                              ir::BasicBlock *trueBlock,
                              ir::BasicBlock *falseBlock) {
  if (ctx->OR()) {
    auto *func = builder_.getCurrentFunction();
    auto *nextBlock = func->createBasicBlock("lor.rhs");
    emitLor(ctx->lorExp(), trueBlock, nextBlock);
    builder_.setInsertPoint(nextBlock);
    emitLand(ctx->landExp(), trueBlock, falseBlock);
  } else {
    emitLand(ctx->landExp(), trueBlock, falseBlock);
  }
}

void SysYIRGenerator::emitLand(SysYParser::LandExpContext *ctx,
                               ir::BasicBlock *trueBlock,
                               ir::BasicBlock *falseBlock) {
  if (ctx->AND()) {
    auto *func = builder_.getCurrentFunction();
    auto *nextBlock = func->createBasicBlock("land.rhs");
    emitLand(ctx->landExp(), nextBlock, falseBlock);
    builder_.setInsertPoint(nextBlock);
    emitEq(ctx->eqExp(), trueBlock, falseBlock);
  } else {
    emitEq(ctx->eqExp(), trueBlock, falseBlock);
  }
}

void SysYIRGenerator::emitEq(SysYParser::EqExpContext *ctx,
                             ir::BasicBlock *trueBlock,
                             ir::BasicBlock *falseBlock) {
  if (ctx->EQ() || ctx->NEQ()) {
    auto *lhs = evaluateEqAsInt(ctx->eqExp());
    auto *rhs = evaluateRelAsInt(ctx->relExp());
    lhs = ensureInteger(lhs);
    rhs = ensureInteger(rhs);
    auto op = ctx->EQ() ? ir::CmpOp::EQ : ir::CmpOp::NE;
    auto *cmp = builder_.createICmp(op, lhs, rhs);
    builder_.createCondBr(cmp, trueBlock, falseBlock);
  } else {
    emitRel(ctx->relExp(), trueBlock, falseBlock);
  }
}

void SysYIRGenerator::emitRel(SysYParser::RelExpContext *ctx,
                              ir::BasicBlock *trueBlock,
                              ir::BasicBlock *falseBlock) {
  if (ctx->LT() || ctx->GT() || ctx->LE() || ctx->GE()) {
    auto *lhs = evaluateRelAsInt(ctx->relExp());
    auto *rhs = evaluateAddExp(ctx->addExp());
    lhs = ensureInteger(lhs);
    rhs = ensureInteger(rhs);
    ir::CmpOp op;
    if (ctx->LT()) op = ir::CmpOp::LT;
    else if (ctx->GT()) op = ir::CmpOp::GT;
    else if (ctx->LE()) op = ir::CmpOp::LE;
    else op = ir::CmpOp::GE;
    auto *cmp = builder_.createICmp(op, lhs, rhs);
    builder_.createCondBr(cmp, trueBlock, falseBlock);
  } else {
    auto *val = evaluateAddExp(ctx->addExp());
    val = ensureInteger(val);
    auto *cmp = builder_.createICmp(ir::CmpOp::NE, val, builder_.getInt32(0).get());
    builder_.createCondBr(cmp, trueBlock, falseBlock);
  }
}

ir::Value *SysYIRGenerator::evaluateExp(SysYParser::ExpContext *ctx) {
  return evaluateAddExp(ctx->addExp());
}

ir::Value *SysYIRGenerator::evaluateAddExp(SysYParser::AddExpContext *ctx) {
  if (ctx->ADD() || ctx->SUB()) {
    auto *lhs = evaluateAddExp(ctx->addExp());
    auto *rhs = evaluateMulExp(ctx->mulExp());
    lhs = ensureInteger(lhs);
    rhs = ensureInteger(rhs);
    auto op = ctx->ADD() ? ir::BinaryOp::Add : ir::BinaryOp::Sub;
    return builder_.createBinary(op, lhs, rhs);
  } else {
    return evaluateMulExp(ctx->mulExp());
  }
}

ir::Value *SysYIRGenerator::evaluateMulExp(SysYParser::MulExpContext *ctx) {
  if (ctx->MUL() || ctx->DIV() || ctx->MOD()) {
    auto *lhs = evaluateMulExp(ctx->mulExp());
    auto *rhs = evaluateUnaryExp(ctx->unaryExp());
    lhs = ensureInteger(lhs);
    rhs = ensureInteger(rhs);
    ir::BinaryOp op;
    if (ctx->MUL()) op = ir::BinaryOp::Mul;
    else if (ctx->DIV()) op = ir::BinaryOp::SDiv;
    else op = ir::BinaryOp::SRem;
    return builder_.createBinary(op, lhs, rhs);
  } else {
    return evaluateUnaryExp(ctx->unaryExp());
  }
}

ir::Value *SysYIRGenerator::evaluateUnaryExp(SysYParser::UnaryExpContext *ctx) {
  if (ctx->primaryExp()) {
    return evaluatePrimaryExp(ctx->primaryExp());
  } else if (ctx->IDENT()) {
    auto funcName = ctx->IDENT()->getText();
    auto *func = module_.getFunction(funcName);
    if (!func) {
      throw std::runtime_error("undefined function: " + funcName);
    }
    std::vector<ir::Value *> args;
    if (ctx->funcRParams()) {
      auto paramTypes = func->getFunctionType()->getParamTypes();
      auto argExprs = ctx->funcRParams()->exp();
      for (std::size_t i = 0; i < argExprs.size(); ++i) {
        ir::TypePtr expectedType = nullptr;
        if (i < paramTypes.size()) {
          expectedType = paramTypes[i];
        }
        args.push_back(evaluateForArgument(argExprs[i], expectedType));
      }
    }
    return builder_.createCall(func, args);
  } else if (ctx->unaryExp()) {
    auto *val = evaluateUnaryExp(ctx->unaryExp());
    val = ensureInteger(val);
    if (ctx->ADD()) {
      return val;
    } else if (ctx->SUB()) {
      return builder_.createBinary(ir::BinaryOp::Sub, builder_.getInt32(0).get(), val);
    } else if (ctx->NOT()) {
      auto *cmp = builder_.createICmp(ir::CmpOp::EQ, val, builder_.getInt32(0).get());
      return builder_.createZExt(cmp, ir::Type::getInt32());
    }
  }
  return nullptr;
}

ir::Value *SysYIRGenerator::evaluatePrimaryExp(SysYParser::PrimaryExpContext *ctx) {
  if (ctx->exp()) {
    return evaluateExp(ctx->exp());
  } else if (ctx->lVal()) {
    return evaluateLVal(ctx->lVal());
  } else if (ctx->number()) {
    return builder_.getInt32(parseIntegerLiteral(ctx->number()->getText())).get();
  }
  return nullptr;
}

ir::Value *SysYIRGenerator::evaluateLVal(SysYParser::LValContext *ctx) {
  auto *addr = getLValAddress(ctx);
  auto ptrType = std::dynamic_pointer_cast<ir::PointerType>(addr->getType());
  if (ptrType->getElementType()->isArray()) {
    std::vector<ir::Value *> indices = {builder_.getInt32(0).get(), builder_.getInt32(0).get()};
    return builder_.createGEP(ptrType->getElementType(), addr, indices);
  } else {
    return builder_.createLoad(ptrType->getElementType(), addr);
  }
}

ir::Value *SysYIRGenerator::getArrayDecayPointer(const Symbol &symbol, ir::Value *ptr,
                                  std::size_t usedIndices,
                                  ir::TypePtr expectedType) {
  return ptr;
}

SysYParser::LValContext *SysYIRGenerator::tryExtractLVal(SysYParser::ExpContext *ctx) {
  auto *add = ctx->addExp();
  if (add->ADD() || add->SUB()) return nullptr;
  auto *mul = add->mulExp();
  if (mul->MUL() || mul->DIV() || mul->MOD()) return nullptr;
  auto *unary = mul->unaryExp();
  if (unary->unaryExp() || unary->IDENT()) return nullptr;
  auto *primary = unary->primaryExp();
  if (primary->lVal()) return primary->lVal();
  return nullptr;
}

ir::Value *SysYIRGenerator::evaluateEqAsInt(SysYParser::EqExpContext *ctx) {
  if (ctx->EQ() || ctx->NEQ()) {
    auto *lhs = evaluateEqAsInt(ctx->eqExp());
    auto *rhs = evaluateRelAsInt(ctx->relExp());
    lhs = ensureInteger(lhs);
    rhs = ensureInteger(rhs);
    auto op = ctx->EQ() ? ir::CmpOp::EQ : ir::CmpOp::NE;
    auto *cmp = builder_.createICmp(op, lhs, rhs);
    return builder_.createZExt(cmp, ir::Type::getInt32());
  } else {
    return evaluateRelAsInt(ctx->relExp());
  }
}

ir::Value *SysYIRGenerator::evaluateRelAsInt(SysYParser::RelExpContext *ctx) {
  if (ctx->LT() || ctx->GT() || ctx->LE() || ctx->GE()) {
    auto *lhs = evaluateRelAsInt(ctx->relExp());
    auto *rhs = evaluateAddExp(ctx->addExp());
    lhs = ensureInteger(lhs);
    rhs = ensureInteger(rhs);
    ir::CmpOp op;
    if (ctx->LT()) op = ir::CmpOp::LT;
    else if (ctx->GT()) op = ir::CmpOp::GT;
    else if (ctx->LE()) op = ir::CmpOp::LE;
    else op = ir::CmpOp::GE;
    auto *cmp = builder_.createICmp(op, lhs, rhs);
    return builder_.createZExt(cmp, ir::Type::getInt32());
  } else {
    return evaluateAddExp(ctx->addExp());
  }
}

ir::Value *SysYIRGenerator::getLValAddress(SysYParser::LValContext *ctx) {
  auto name = ctx->IDENT()->getText();
  auto *symbol = symbols_.lookup(name);
  if (!symbol) {
    throw std::runtime_error("undefined variable: " + name);
  }
  
    ir::Value *currentPtr = symbol->address;
    auto ptrType = std::dynamic_pointer_cast<ir::PointerType>(currentPtr->getType());
    auto elemType = ptrType->getElementType();
  
  std::vector<ir::Value *> indices;
  for (auto *exp : ctx->exp()) {
    indices.push_back(ensureInteger(evaluateExp(exp)));
  }

  if (symbol->isPointerParam) {
    if (indices.empty()) return currentPtr; // already a pointer to element type
    std::vector<ir::Value *> args;
    for (auto *idx : indices) args.push_back(idx);
    auto ptrElemType = std::dynamic_pointer_cast<ir::PointerType>(currentPtr->getType())->getElementType();
    return builder_.createGEP(ptrElemType, currentPtr, args);
  } else {
    if (indices.empty()) return currentPtr;
    std::vector<ir::Value *> args;
    args.push_back(builder_.getInt32(0).get());
    for (auto *idx : indices) args.push_back(idx);
    auto ptrElemType = std::dynamic_pointer_cast<ir::PointerType>(currentPtr->getType())->getElementType();
    return builder_.createGEP(ptrElemType, currentPtr, args);
  }
}

ir::Value *SysYIRGenerator::evaluateForArgument(SysYParser::ExpContext *ctx, ir::TypePtr expectedType) {
  // Support array-to-pointer decay when the callee expects a pointer type.
  if (expectedType && expectedType->isPointer()) {
    if (auto *lvalCtx = tryExtractLVal(ctx)) {
      auto *addr = getLValAddress(lvalCtx);
      auto addrType = std::dynamic_pointer_cast<ir::PointerType>(addr->getType());
      auto elemType = addrType->getElementType();

      // If we have an array lvalue and the callee expects a pointer, decay to
      // the first element pointer (GEP 0,0,...)
      if (elemType->isArray()) {
        std::vector<ir::Value *> decayIdx = {builder_.getInt32(0).get(), builder_.getInt32(0).get()};
        return builder_.createGEP(elemType, addr, decayIdx);
      }

      // If the lvalue already matches (pointer parameter), just use the
      // computed address.
      return addr;
    }
  }

  return evaluateExp(ctx);
}

ir::Value *SysYIRGenerator::ensureInteger(ir::Value *value) {
  if (value->getType()->isInteger() && std::dynamic_pointer_cast<ir::IntegerType>(value->getType())->getBitWidth() == 1) {
    return builder_.createZExt(value, ir::Type::getInt32());
  }
  return value;
}

ir::Value *SysYIRGenerator::ensureBoolean(ir::Value *value) {
  if (value->getType()->isInteger() && std::dynamic_pointer_cast<ir::IntegerType>(value->getType())->getBitWidth() == 32) {
    return builder_.createICmp(ir::CmpOp::NE, value, builder_.getInt32(0).get());
  }
  return value;
}

int SysYIRGenerator::evalConstExp(SysYParser::ConstExpContext *ctx) {
  return evalAddExp(ctx->addExp());
}

int SysYIRGenerator::evalAddExp(SysYParser::AddExpContext *ctx) {
  if (ctx->ADD() || ctx->SUB()) {
    int lhs = evalAddExp(ctx->addExp());
    int rhs = evalMulExp(ctx->mulExp());
    return ctx->ADD() ? lhs + rhs : lhs - rhs;
  }
  return evalMulExp(ctx->mulExp());
}

int SysYIRGenerator::evalMulExp(SysYParser::MulExpContext *ctx) {
  if (ctx->MUL() || ctx->DIV() || ctx->MOD()) {
    int lhs = evalMulExp(ctx->mulExp());
    int rhs = evalUnaryConst(ctx->unaryExp());
    if (ctx->MUL()) return lhs * rhs;
    if (ctx->DIV()) return rhs != 0 ? lhs / rhs : 0;
    return rhs != 0 ? lhs % rhs : 0;
  }
  return evalUnaryConst(ctx->unaryExp());
}

int SysYIRGenerator::evalUnaryConst(SysYParser::UnaryExpContext *ctx) {
  if (ctx->primaryExp()) {
    return evalPrimaryConst(ctx->primaryExp());
  } else if (ctx->unaryExp()) {
    int val = evalUnaryConst(ctx->unaryExp());
    if (ctx->ADD()) return val;
    if (ctx->SUB()) return -val;
    if (ctx->NOT()) return !val;
  }
  return 0;
}

int SysYIRGenerator::evalPrimaryConst(SysYParser::PrimaryExpContext *ctx) {
  if (ctx->exp()) {
    return evalAddExp(ctx->exp()->addExp());
  } else if (ctx->lVal()) {
    return evalConstLVal(ctx->lVal());
  } else if (ctx->number()) {
    return parseIntegerLiteral(ctx->number()->getText());
  }
  return 0;
}

int SysYIRGenerator::evalConstLVal(SysYParser::LValContext *ctx) {
  auto name = ctx->IDENT()->getText();
  auto *symbol = symbols_.lookup(name);
  if (!symbol || !symbol->isConst) {
    throw std::runtime_error("constant expression must refer to constant: " + name);
  }
  
  std::vector<int> indices;
  for (auto *exp : ctx->exp()) {
    indices.push_back(evalAddExp(exp->addExp()));
  }
  
  // Calculate linear index
  std::size_t linearIndex = 0;
  std::size_t stride = 1;
  // Row-major: index = i * dim1 * dim2 + j * dim2 + k
  // But we have dims [d0, d1, d2].
  // idx0 * (d1*d2) + idx1 * (d2) + idx2.
  
  // We need strides.
  // strides[i] = product(dims[i+1]...dims[n-1])
  
  if (indices.size() != symbol->dimensions.size()) {
     // Partial indexing not allowed in const eval usually?
     // Or maybe it is if it refers to an array? But evalConstLVal returns int.
     // So it must be full indexing.
     throw std::runtime_error("partial indexing in constant expression");
  }
  
  for (size_t i = 0; i < indices.size(); ++i) {
     std::size_t subSize = 1;
     for (size_t j = i + 1; j < symbol->dimensions.size(); ++j) {
       subSize *= symbol->dimensions[j];
     }
     linearIndex += indices[i] * subSize;
  }
  
  if (linearIndex < symbol->constData.size()) {
    return symbol->constData[linearIndex];
  }
  return 0;
}

SysYIRGenerator::ConstValueList SysYIRGenerator::flattenConstInit(SysYParser::ConstInitValContext *ctx) {
  ConstValueList list;
  if (ctx->constExp()) {
    list.push_back(evalConstExp(ctx->constExp()));
  } else {
    for (auto *child : ctx->constInitVal()) {
      auto subList = flattenConstInit(child);
      list.insert(list.end(), subList.begin(), subList.end());
    }
  }
  return list;
}

SysYIRGenerator::ConstValueList
SysYIRGenerator::materializeConstInit(SysYParser::ConstInitValContext *ctx,
                                      const std::vector<int> &dims) {
  std::size_t total = totalSize(dims);
  ConstValueList values(total, 0);
  if (!ctx) {
    return values;
  }

  std::function<void(SysYParser::ConstInitValContext *, std::size_t, std::size_t &)> traverse =
      [&](SysYParser::ConstInitValContext *node, std::size_t dimIndex, std::size_t &cursor) {
        if (node->constExp()) {
          if (cursor < values.size()) {
            values[cursor++] = evalConstExp(node->constExp());
          }
          return;
        }

        std::size_t subSize = 1;
        for (std::size_t k = dimIndex + 1; k < dims.size(); ++k) {
          subSize *= static_cast<std::size_t>(dims[k]);
        }

        std::size_t childDimIndex = dimIndex + 1;
        for (auto *child : node->constInitVal()) {
          if (child->constExp()) {
            if (cursor < values.size()) {
              values[cursor++] = evalConstExp(child->constExp());
            }
          } else {
            std::size_t currentPos = subSize ? (cursor % subSize) : 0;
            if (currentPos != 0) {
              cursor += (subSize - currentPos);
            }

            std::size_t startCursor = cursor;
            traverse(child, childDimIndex, cursor);

            std::size_t endCursor = startCursor + subSize;
            if (cursor < endCursor) {
              cursor = endCursor;
            }
          }
        }
      };

  std::size_t cursor = 0;
  traverse(ctx, 0, cursor);
  return values;
}

SysYIRGenerator::ConstValueList
SysYIRGenerator::materializeInit(SysYParser::InitValContext *ctx,
                                 const std::vector<int> &dims) {
  std::size_t total = totalSize(dims);
  ConstValueList values(total, 0);
  if (!ctx) {
    return values;
  }

  std::function<void(SysYParser::InitValContext *, std::size_t, std::size_t &)> traverse =
      [&](SysYParser::InitValContext *node, std::size_t dimIndex, std::size_t &cursor) {
        if (node->exp()) {
          if (cursor < values.size()) {
            values[cursor++] = evalAddExp(node->exp()->addExp());
          }
          return;
        }

        std::size_t subSize = 1;
        for (std::size_t k = dimIndex + 1; k < dims.size(); ++k) {
          subSize *= static_cast<std::size_t>(dims[k]);
        }

        std::size_t childDimIndex = dimIndex + 1;
        for (auto *child : node->initVal()) {
          if (child->exp()) {
            if (cursor < values.size()) {
              values[cursor++] = evalAddExp(child->exp()->addExp());
            }
          } else {
            std::size_t currentPos = subSize ? (cursor % subSize) : 0;
            if (currentPos != 0) {
              cursor += (subSize - currentPos);
            }

            std::size_t startCursor = cursor;
            traverse(child, childDimIndex, cursor);

            std::size_t endCursor = startCursor + subSize;
            if (cursor < endCursor) {
              cursor = endCursor;
            }
          }
        }
      };

  std::size_t cursor = 0;
  traverse(ctx, 0, cursor);
  return values;
}

void SysYIRGenerator::normalizeInitializer(ConstValueList &values, std::size_t total) {
  if (values.size() < total) {
    values.resize(total, 0);
  }
}

std::shared_ptr<ir::Constant> SysYIRGenerator::buildArrayConstant(const ir::TypePtr &type, ConstValueList &values, std::size_t &cursor) {
  auto arrayType = std::dynamic_pointer_cast<ir::ArrayType>(type);
  std::vector<std::shared_ptr<ir::Constant>> elems;
  auto elemType = arrayType->getElementType();
  for (size_t i = 0; i < arrayType->getElementCount(); ++i) {
    if (elemType->isArray()) {
      elems.push_back(buildArrayConstant(elemType, values, cursor));
    } else {
      elems.push_back(ir::ConstantInt::get(values[cursor++]));
    }
  }
  return std::make_shared<ir::ConstantArray>(arrayType, elems);
}

void SysYIRGenerator::emitArrayInitializer(ir::Value *basePtr, const std::vector<int> &dims, SysYParser::InitValContext *ctx) {
  size_t total = totalSize(dims);
  for (size_t i = 0; i < total; ++i) {
    storeLinearElement(basePtr, dims, i, builder_.getInt32(0).get());
  }
  
  std::function<void(SysYParser::InitValContext*, size_t, size_t&)> traverse = 
    [&](SysYParser::InitValContext *node, size_t dimIndex, size_t &cursor) {
      if (node->exp()) {
        storeLinearElement(basePtr, dims, cursor++, ensureInteger(evaluateExp(node->exp())));
      } else {
        size_t subSize = 1;
        for (size_t k = dimIndex + 1; k < dims.size(); ++k) subSize *= dims[k];
        
        size_t childDimIndex = dimIndex + 1;
        
        for (auto *child : node->initVal()) {
           if (child->exp()) {
             storeLinearElement(basePtr, dims, cursor++, ensureInteger(evaluateExp(child->exp())));
           } else {
             size_t currentPosInElem = cursor % subSize;
             if (currentPosInElem != 0) {
                 cursor += (subSize - currentPosInElem);
             }
             
             size_t startCursor = cursor;
             traverse(child, childDimIndex, cursor);
             
             size_t endCursor = startCursor + subSize;
             if (cursor < endCursor) {
                 cursor = endCursor;
             }
           }
        }
      }
  };
  
  size_t cursor = 0;
  traverse(ctx, 0, cursor);
}

void SysYIRGenerator::storeLinearElement(ir::Value *basePtr, const std::vector<int> &dims, std::size_t linearIndex, ir::Value *value) {
  auto coords = linearIndexToCoords(linearIndex, dims);
  std::vector<ir::Value *> indices;
  indices.push_back(builder_.getInt32(0).get());
  for (int c : coords) {
    indices.push_back(builder_.getInt32(c).get());
  }
  
  // basePtr is [d0 x [d1 x ...]]*
  // GEP 0, c0, c1...
  
  // We need to construct the GEP.
  // We need the type of basePtr.
  auto ptrType = std::dynamic_pointer_cast<ir::PointerType>(basePtr->getType());
  auto elemType = ptrType->getElementType();
  
  // We need to peel types as we go?
  // createGEP handles it if we pass all indices.
  // But we need the final element type?
  // createGEP takes the type of the POINTER's element.
  
  auto *gep = builder_.createGEP(elemType, basePtr, indices);
  builder_.createStore(value, gep);
}

std::vector<int> SysYIRGenerator::linearIndexToCoords(std::size_t linear, const std::vector<int> &dims) const {
  std::vector<int> coords(dims.size());
  for (int i = static_cast<int>(dims.size()) - 1; i >= 0; --i) {
    coords[i] = linear % dims[i];
    linear /= dims[i];
  }
  return coords;
}

} // namespace compiler
