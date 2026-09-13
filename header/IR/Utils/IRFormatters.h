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
std::string FormatTAC(TAC& TAC, bool history = false, bool metadata = false);
std::string FormatMIR(MIR& MIR, bool history = false, bool metadata = false);
std::string GetASM(std::string AsmFilePath);

inline std::string FormatIR(TokenStream& tokenStream, bool history = false, bool meta = false) { return FormatTokens(tokenStream); }
inline std::string FormatIR(Node& AST, bool history = false, bool meta = false) { return FormatAST(AST); }
inline std::string FormatIR(CFG& CFG, bool history = false, bool meta = false) { return FormatCFG(CFG); }
inline std::string FormatIR(TAC& TAC, bool history = false, bool meta = false) { return FormatTAC(TAC, history, meta); }
inline std::string FormatIR(MIR& MIR, bool history = false, bool meta = false) { return FormatMIR(MIR, history, meta); }
inline std::string FormatIR(std::string AsmFilePath, bool history = false, bool meta = false) { return GetASM(AsmFilePath); }
