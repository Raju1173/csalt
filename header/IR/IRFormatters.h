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
std::string FormatTAC(TAC& TAC);
std::string FormatMIR(MIR& MIR);
std::string FormatASM(std::string AsmFilePath);

inline std::string FormatIR(TokenStream& tokenStream) { FormatTokens(tokenStream); }
inline std::string FormatIR(Node& AST) { FormatAST(AST); }
inline std::string FormatIR(CFG& CFG) { FormatCFG(CFG); }
inline std::string FormatIR(TAC& TAC) { FormatTAC(TAC); }
inline std::string FormatIR(MIR& MIR) { FormatMIR(MIR); }
inline std::string FormatIR(std::string AsmFilePath) { FormatASM(AsmFilePath); }
