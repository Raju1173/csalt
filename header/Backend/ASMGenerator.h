#include "MIRGenerator.h"

void EmitAssembly(MIR& MIR, const std::string& filename);

void PrintASM(std::string AssemblyFilePath);

void EmitExecutable(std::string ASMFilePath, std::string ExecFileName);

void PrintOutput(std::string ExecFilePath);
