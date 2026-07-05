#include "ASMGenerator.h"
#include "CFGBuilder.h"
#include "SSAConstructor.h"
#include "TACGenerator.h"
#include "lexer.h"
#include "parser.h"
#include <fstream>
#include <iostream>
#include <ostream>
#include <print>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::print("Error : Exactly one file name expected\n");

        return 1;
    }

    std::ifstream file(argv[1]);

    if (!file.is_open())
    {
        std::print("Error : Could not open file\n");
        return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());

    std::vector<Token> TokenStream = tokenize(source);

    std::print("------TOKENS-------\n\n");

    for (Token t : TokenStream)
    {
        if (t.type == TokenType::IDENTIFIER || t.type == TokenType::NUMBER)
            std::print("{}({})\n", TokenNames[std::to_underlying(t.type)], t.lexeme);
        else
            std::print("{}\n", TokenNames[std::to_underlying(t.type)]);
    }

    std::print("\n------AST-------\n\n");

    Node AST = parse(TokenStream);

    printNode(AST);

    std::print("\n------CFG-------\n");

    std::vector<std::unique_ptr<CFGFunction>> CFG = constructCFG(AST);

    ComputeDominators(CFG);

    ComputeDominatorTree(CFG);

    for (auto& CFGFunc : CFG)
    {
        ComputeFrontiers(CFGFunc->Blocks[0].get());
    }

    InsertPhiNodes(CFG);

    RenameVariables(CFG);

    printCFG(CFG);

    std::print("\n------TAC-------\n\n");

    auto TAC = GenerateTAC(CFG);

    ResolvePhiNodes(TAC);

    printTAC(TAC);

    std::print("\n------ASSEMBLY-----\n\n");

    auto MIR = GenerateMachineIR(TAC);

    std::string AssemblyFilePath = std::string(argv[1], 0, std::strlen(argv[1]) - 1) + "s";

    EmitAssembly(MIR, AssemblyFilePath);

    std::ifstream AsmFile(AssemblyFilePath, std::ios::in | std::ios::binary | std::ios::ate);

    std::streamsize size = AsmFile.tellg();
    AsmFile.seekg(0, std::ios::beg);

    std::string AsmOutput(size, '\0');

    AsmFile.read(AsmOutput.data(), size);

    std::print("{}", AsmOutput);

    std::print("\n------OUTPUT-----\n\n");

    std::string ExecutableFilePath = std::string(argv[1], 0, std::strlen(argv[1]) - 2);

    EmitExecutable(AssemblyFilePath, ExecutableFilePath);

    PrintOutput(ExecutableFilePath);

    return 0;
}
