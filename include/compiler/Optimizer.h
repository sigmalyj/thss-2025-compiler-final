// 机器无关简单优化：常量传播 + 无用代码删除
#pragma once

#include "ir/Module.h"

namespace compiler {

void runOptimizations(ir::Module &module);

}
