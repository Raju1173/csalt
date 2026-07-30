#include "lexer.h"
#include "parser.h"
#include "CFGBuilder.h"
#include "TACGenerator.h"
#include "MIRGenerator.h"
#include "ASMGenerator.h"
#include <string>

std::string FormatTokens(TokenStream& TokenStream);
std::string FormatAST(Node& AST);
std::string FormatCFG(CFG& CFG);
std::string FormatTAC(TAC& TAC, bool history = false);
std::string FormatMIR(MIR& MIR, bool history = false);
std::string GetASM(std::string AsmFilePath);

inline std::string FormatIR(TokenStream& tokenStream) { return FormatTokens(tokenStream); }
inline std::string FormatIR(Node& AST) { return FormatAST(AST); }
inline std::string FormatIR(CFG& CFG) { return FormatCFG(CFG); }
inline std::string FormatIR(TAC& TAC, bool history = false) { return FormatTAC(TAC, history); }
inline std::string FormatIR(MIR& MIR, bool history = false) { return FormatMIR(MIR, history); }
inline std::string FormatIR(std::string AsmFilePath) { return GetASM(AsmFilePath); }
