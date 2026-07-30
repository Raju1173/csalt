#include "MIRGenerator.h"

void EmitAssembly(MIR& MIR, const std::string& filename);

void EmitExecutable(std::string ASMFilePath, std::string ExecFileName);

void PrintOutput(std::string ExecFilePath);
