#include "TACGenerator.h"
#include <memory>
#include <string_view>
#include <vector>

bool isConstant(std::string_view str);

void FoldConstants(std::vector<std::unique_ptr<TACFunction>>& TAC);
