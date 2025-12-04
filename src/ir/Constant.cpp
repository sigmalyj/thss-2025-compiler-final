#include "ir/Constant.h"

#include <map>
#include <sstream>
#include <utility>

namespace ir {

namespace {

std::map<int, std::shared_ptr<ConstantInt>> &intCache() {
  static std::map<int, std::shared_ptr<ConstantInt>> cache;
  return cache;
}

} // namespace

ConstantInt::ConstantInt(int value)
    : Constant(Type::getInt32()), value_(value) {}

std::shared_ptr<ConstantInt> ConstantInt::get(int value) {
  auto &cache = intCache();
  if (auto it = cache.find(value); it != cache.end()) {
    return it->second;
  }
  auto created = std::shared_ptr<ConstantInt>(new ConstantInt(value));
  cache[value] = created;
  return created;
}

std::string ConstantInt::asOperand() const {
  return std::to_string(value_);
}

ConstantZero::ConstantZero(TypePtr type)
    : Constant(std::move(type)) {}

ConstantArray::ConstantArray(ArrayTypePtr type,
                             std::vector<std::shared_ptr<Constant>> elems)
    : Constant(std::move(type)), elements_(std::move(elems)) {}

std::string ConstantArray::asOperand() const {
  auto arrayType = std::dynamic_pointer_cast<ArrayType>(getType());
  if (!arrayType) {
    return "";
  }
  std::ostringstream oss;
  oss << '[';
  for (std::size_t i = 0; i < elements_.size(); ++i) {
    if (i != 0) {
      oss << ", ";
    }
    oss << arrayType->getElementType()->str() << ' ' << elements_[i]->asOperand();
  }
  oss << ']';
  return oss.str();
}

} // namespace ir
