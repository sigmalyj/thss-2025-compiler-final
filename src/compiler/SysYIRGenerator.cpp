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
    values = flattenConstInit(ctx->constInitVal());
  }
  normalizeInitializer(values, total);

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
      ConstValueList values;
      auto flatten = [&](auto &&self, SysYParser::InitValContext *node) -> void {
        if (node->exp()) {
          values.push_back(evalConstExp(node->exp()->addExp()->constExp()));
        } else {
          for (auto *child : node->initVal()) {
            self(self, child);
          }
        }
      };
      // Placeholder: global variable init requires constant expressions
      flatten(flatten, ctx->initVal());
      normalizeInitializer(values, total);
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

// ... (Remaining member function implementations will follow here)

} // namespace compiler
