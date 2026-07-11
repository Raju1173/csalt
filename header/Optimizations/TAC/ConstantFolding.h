#include "TACGenerator.h"
#include <string_view>

bool isConstant(std::string_view str);

bool FoldConstants(TAC& TAC);
