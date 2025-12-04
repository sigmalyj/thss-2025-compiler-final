#pragma once

#include "SysYParser.h"
#include "compiler/SymbolTable.h"
#include "ir/IRBuilder.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace compiler {

class SysYIRGenerator {
public:
  SysYIRGenerator(ir::Module &module, ir::IRBuilder &builder);

  void generate(SysYParser::CompUnitContext *ctx);

private:
  using ConstValueList = std::vector<int>;

  bool isGlobalScope() const;

  void handleDecl(SysYParser::DeclContext *ctx);
  void handleConstDecl(SysYParser::ConstDeclContext *ctx);
  void handleVarDecl(SysYParser::VarDeclContext *ctx);

  void handleConstDef(SysYParser::ConstDefContext *ctx);
  void handleVarDef(SysYParser::VarDefContext *ctx);

  void emitBlock(SysYParser::BlockContext *ctx);
  void emitStmt(SysYParser::StmtContext *ctx);

  void emitCondition(SysYParser::CondContext *ctx, ir::BasicBlock *trueBlock,
                     ir::BasicBlock *falseBlock);
  void emitLor(SysYParser::LorExpContext *ctx, ir::BasicBlock *trueBlock,
               ir::BasicBlock *falseBlock);
  void emitLand(SysYParser::LandExpContext *ctx, ir::BasicBlock *trueBlock,
                ir::BasicBlock *falseBlock);
  void emitEq(SysYParser::EqExpContext *ctx, ir::BasicBlock *trueBlock,
              ir::BasicBlock *falseBlock);
  void emitRel(SysYParser::RelExpContext *ctx, ir::BasicBlock *trueBlock,
               ir::BasicBlock *falseBlock);

  ir::Value *evaluateExp(SysYParser::ExpContext *ctx);
  ir::Value *evaluateAddExp(SysYParser::AddExpContext *ctx);
  ir::Value *evaluateMulExp(SysYParser::MulExpContext *ctx);
  ir::Value *evaluateUnaryExp(SysYParser::UnaryExpContext *ctx);
  ir::Value *evaluatePrimaryExp(SysYParser::PrimaryExpContext *ctx);
  ir::Value *evaluateEqAsInt(SysYParser::EqExpContext *ctx);
  ir::Value *evaluateRelAsInt(SysYParser::RelExpContext *ctx);

  ir::Value *evaluateLVal(SysYParser::LValContext *ctx);
  ir::Value *getLValAddress(SysYParser::LValContext *ctx);
  ir::Value *getArrayDecayPointer(const Symbol &symbol, ir::Value *ptr,
                                  std::size_t usedIndices,
                                  ir::TypePtr expectedType = nullptr);

  ir::Value *evaluateForArgument(SysYParser::ExpContext *ctx,
                                 ir::TypePtr expectedType);

  ir::Value *ensureBoolean(ir::Value *value);
  ir::Value *ensureInteger(ir::Value *value);

  int evalConstExp(SysYParser::ConstExpContext *ctx);
  int evalAddExp(SysYParser::AddExpContext *ctx);
  int evalMulExp(SysYParser::MulExpContext *ctx);
  int evalUnaryConst(SysYParser::UnaryExpContext *ctx);
  int evalPrimaryConst(SysYParser::PrimaryExpContext *ctx);
  int evalConstLVal(SysYParser::LValContext *ctx);

  std::vector<int> collectDimensions(
      const std::vector<SysYParser::ConstExpContext *> &contexts);
  std::size_t totalSize(const std::vector<int> &dims) const;

  ConstValueList flattenConstInit(SysYParser::ConstInitValContext *ctx);
  void normalizeInitializer(ConstValueList &values, std::size_t total);

  std::shared_ptr<ir::Constant>
  buildArrayConstant(const ir::TypePtr &type, ConstValueList &values,
                     std::size_t &cursor);

  void emitArrayInitializer(ir::Value *basePtr, const std::vector<int> &dims,
                            SysYParser::InitValContext *ctx);
  void storeLinearElement(ir::Value *basePtr, const std::vector<int> &dims,
                          std::size_t linearIndex, ir::Value *value);
  std::vector<int> linearIndexToCoords(std::size_t linear,
                                       const std::vector<int> &dims) const;

  bool isCurrentBlockTerminated() const;

  SysYParser::LValContext *tryExtractLVal(SysYParser::ExpContext *ctx);

  ir::TypePtr buildArrayType(const std::vector<int> &dims) const;

  template <typename T> void assignConstData(Symbol &symbol, T &&data);

  ir::Module &module_;
  ir::IRBuilder &builder_;
  SymbolTable symbols_;
  std::vector<std::pair<ir::BasicBlock *, ir::BasicBlock *>> loopStack_;
};

template <typename T>
void SysYIRGenerator::assignConstData(Symbol &symbol, T &&data) {
  symbol.constData = std::forward<T>(data);
}

} // namespace compiler
