#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace ir {

class Type;
class VoidType;
class IntegerType;
class PointerType;
class ArrayType;
class FunctionType;

using TypePtr = std::shared_ptr<Type>;
using VoidTypePtr = std::shared_ptr<VoidType>;
using IntegerTypePtr = std::shared_ptr<IntegerType>;
using PointerTypePtr = std::shared_ptr<PointerType>;
using ArrayTypePtr = std::shared_ptr<ArrayType>;
using FunctionTypePtr = std::shared_ptr<FunctionType>;

class Type : public std::enable_shared_from_this<Type> {
public:
  enum class Kind { Void, Integer, Pointer, Array, Function };

  explicit Type(Kind kind) : kind_(kind) {}
  virtual ~Type() = default;

  Kind getKind() const { return kind_; }
  bool isVoid() const { return kind_ == Kind::Void; }
  bool isInteger() const { return kind_ == Kind::Integer; }
  bool isPointer() const { return kind_ == Kind::Pointer; }
  bool isArray() const { return kind_ == Kind::Array; }
  bool isFunction() const { return kind_ == Kind::Function; }

  virtual std::string str() const = 0;

  static IntegerTypePtr getInt1();
  static IntegerTypePtr getInt32();
  static TypePtr getVoid();
  static PointerTypePtr getPointer(TypePtr elementType);
  static ArrayTypePtr getArray(TypePtr elementType, std::size_t elementCount);
  static FunctionTypePtr getFunction(TypePtr returnType,
                                     const std::vector<TypePtr> &params,
                                     bool isVarArg = false);

private:
  Kind kind_;
};

class IntegerType : public Type {
public:
  explicit IntegerType(unsigned bitWidth);

  unsigned getBitWidth() const { return bitWidth_; }
  std::string str() const override;

private:
  unsigned bitWidth_;
};

class VoidType : public Type {
public:
  VoidType();
  std::string str() const override;
};

class PointerType : public Type {
public:
  explicit PointerType(TypePtr elementType);

  const TypePtr &getElementType() const { return elementType_; }
  std::string str() const override;

private:
  TypePtr elementType_;
};

class ArrayType : public Type {
public:
  ArrayType(TypePtr elementType, std::size_t elementCount);

  const TypePtr &getElementType() const { return elementType_; }
  std::size_t getElementCount() const { return elementCount_; }
  std::string str() const override;

private:
  TypePtr elementType_;
  std::size_t elementCount_;
};

class FunctionType : public Type {
public:
  FunctionType(TypePtr returnType, std::vector<TypePtr> paramTypes,
               bool isVarArg);

  const TypePtr &getReturnType() const { return returnType_; }
  const std::vector<TypePtr> &getParamTypes() const { return paramTypes_; }
  bool isVarArg() const { return isVarArg_; }
  std::string str() const override;

private:
  TypePtr returnType_;
  std::vector<TypePtr> paramTypes_;
  bool isVarArg_;
};

} // namespace ir
