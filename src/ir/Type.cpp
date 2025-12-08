// 类型系统与缓存实现
#include "ir/Type.h"

#include <map>
#include <sstream>
#include <tuple>
#include <utility>

namespace ir {

namespace {

IntegerTypePtr &singletonInt(unsigned bitWidth) {
  static IntegerTypePtr int1 = std::make_shared<IntegerType>(1);
  static IntegerTypePtr int32 = std::make_shared<IntegerType>(32);
  return bitWidth == 1 ? int1 : int32;
}

VoidTypePtr &singletonVoid() {
  static VoidTypePtr voidType = std::make_shared<VoidType>();
  return voidType;
}

using PointerCacheKey = const Type *;
using ArrayCacheKey = std::pair<const Type *, std::size_t>;
using FunctionCacheKey = std::tuple<const Type *, std::vector<const Type *>,
                                    bool>;

std::map<PointerCacheKey, std::weak_ptr<PointerType>> &pointerCache() {
  static std::map<PointerCacheKey, std::weak_ptr<PointerType>> cache;
  return cache;
}

std::map<ArrayCacheKey, std::weak_ptr<ArrayType>> &arrayCache() {
  static std::map<ArrayCacheKey, std::weak_ptr<ArrayType>> cache;
  return cache;
}

std::map<FunctionCacheKey, std::weak_ptr<FunctionType>> &functionCache() {
  static std::map<FunctionCacheKey, std::weak_ptr<FunctionType>> cache;
  return cache;
}

} // namespace

IntegerType::IntegerType(unsigned bitWidth)
    : Type(Kind::Integer), bitWidth_(bitWidth) {}

std::string IntegerType::str() const {
  std::ostringstream oss;
  oss << 'i' << bitWidth_;
  return oss.str();
}

VoidType::VoidType() : Type(Kind::Void) {}

std::string VoidType::str() const { return "void"; }

PointerType::PointerType(TypePtr elementType)
    : Type(Kind::Pointer), elementType_(std::move(elementType)) {}

std::string PointerType::str() const {
  return elementType_->str() + "*";
}
ArrayType::ArrayType(TypePtr elementType, std::size_t elementCount)
    : Type(Kind::Array), elementType_(std::move(elementType)),
      elementCount_(elementCount) {}

std::string ArrayType::str() const {
  std::ostringstream oss;
  oss << '[' << elementCount_ << " x " << elementType_->str() << ']';
  return oss.str();
}

FunctionType::FunctionType(TypePtr returnType, std::vector<TypePtr> paramTypes,
                           bool isVarArg)
    : Type(Kind::Function), returnType_(std::move(returnType)),
      paramTypes_(std::move(paramTypes)), isVarArg_(isVarArg) {}

std::string FunctionType::str() const {
  std::ostringstream oss;
  oss << returnType_->str() << " (";
  for (std::size_t i = 0; i < paramTypes_.size(); ++i) {
    if (i != 0) {
      oss << ", ";
    }
    oss << paramTypes_[i]->str();
  }
  if (isVarArg_) {
    if (!paramTypes_.empty()) {
      oss << ", ";
    }
    oss << "...";
  }
  oss << ')';
  return oss.str();
}

IntegerTypePtr Type::getInt1() { return singletonInt(1); }

IntegerTypePtr Type::getInt32() { return singletonInt(32); }

TypePtr Type::getVoid() { return singletonVoid(); }

PointerTypePtr Type::getPointer(TypePtr elementType) {
  auto &cache = pointerCache();
  const auto key = elementType.get();
  if (auto it = cache.find(key); it != cache.end()) {
    if (auto locked = it->second.lock()) {
      return locked;
    }
  }
  auto created = std::make_shared<PointerType>(elementType);
  cache[key] = created;
  return created;
}

ArrayTypePtr Type::getArray(TypePtr elementType, std::size_t elementCount) {
  auto &cache = arrayCache();
  ArrayCacheKey key{elementType.get(), elementCount};
  if (auto it = cache.find(key); it != cache.end()) {
    if (auto locked = it->second.lock()) {
      return locked;
    }
  }
  auto created = std::make_shared<ArrayType>(elementType, elementCount);
  cache[key] = created;
  return created;
}

FunctionTypePtr Type::getFunction(TypePtr returnType,
                                  const std::vector<TypePtr> &params,
                                  bool isVarArg) {
  std::vector<const Type *> paramPtrs;
  paramPtrs.reserve(params.size());
  for (const auto &param : params) {
    paramPtrs.push_back(param.get());
  }
  FunctionCacheKey key{returnType.get(), paramPtrs, isVarArg};
  auto &cache = functionCache();
  if (auto it = cache.find(key); it != cache.end()) {
    if (auto locked = it->second.lock()) {
      return locked;
    }
  }
  auto created =
      std::make_shared<FunctionType>(returnType, params, isVarArg);
  cache[key] = created;
  return created;
}

} // namespace ir
