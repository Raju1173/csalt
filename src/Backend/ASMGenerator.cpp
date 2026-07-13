#include "MIRGenerator.h"
#include <fstream>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/resource.h>

void EmitAssembly(MIR& MIR, const std::string& filename)
{
    std::ofstream out(filename);

    out << ".intel_syntax noprefix\n";
    out << ".text\n\n";

    for (const MIRFunction& function : MIR)
    {
        if (function.FunctionName == "main")
            out << ".global main\n";

        out << function.FunctionName << ":\n";

        for (const auto& block : function.Blocks)
        {
            out << "." << function.FunctionName << "L" << block.ID << ":\n";

            for (const auto& inst : block.Instructions)
            {
                switch (inst->type)
                {
                    case MIRType::MOV:
                        {
                            MIRMov* mov = static_cast<MIRMov*>(inst.get());

                            out << "    mov " << OperandString(mov->Dest, function.OmitFramePtr) << ", " << OperandString(mov->Source, function.OmitFramePtr) << "\n";
                        }
                        break;

                    case MIRType::ADD:
                        {
                            MIRAdd* add = static_cast<MIRAdd*>(inst.get());

                            out << "    add " << OperandString(add->Dest, function.OmitFramePtr) << ", " << OperandString(add->Source, function.OmitFramePtr) << "\n";
                        }
                        break;

                    case MIRType::SUB:
                        {
                            MIRSub* sub = static_cast<MIRSub*>(inst.get());

                            out << "    sub " << OperandString(sub->Dest, function.OmitFramePtr) << ", " << OperandString(sub->Source, function.OmitFramePtr) << "\n";
                        }
                        break;

                    case MIRType::MUL:
                        {
                            MIRImul* mul = static_cast<MIRImul*>(inst.get());

                            out << "    imul " << OperandString(mul->Dest, function.OmitFramePtr) << ", " << OperandString(mul->Source, function.OmitFramePtr) << "\n";
                        }
                        break;

                    case MIRType::DIV:
                        {
                            MIRIdiv* div = static_cast<MIRIdiv*>(inst.get());

                            out << "    idiv " << OperandString(div->Divisor, function.OmitFramePtr) << "\n";
                        }
                        break;

                    case MIRType::NEG:
                        {
                            MIRNeg* neg = static_cast<MIRNeg*>(inst.get());

                            out << "    neg " << OperandString(neg->Dest, function.OmitFramePtr) << "\n";
                        }
                        break;

                    case MIRType::CMP:
                        {
                            MIRCmp* cmp = static_cast<MIRCmp*>(inst.get());

                            out << "    cmp " << OperandString(cmp->Left, function.OmitFramePtr) << ", " << OperandString(cmp->Right, function.OmitFramePtr) << "\n";
                        }
                        break;

                    case MIRType::PUSH:
                        {
                            MIRPush* push = static_cast<MIRPush*>(inst.get());

                            out << "    push " << OperandString(push->Source, function.OmitFramePtr) << "\n";
                        }
                        break;

                    case MIRType::POP:
                        {
                            MIRPop* pop = static_cast<MIRPop*>(inst.get());

                            out << "    pop " << OperandString(pop->Dest, function.OmitFramePtr) << "\n";
                        }
                        break;

                    case MIRType::JMP:
                        {
                            MIRJump* jump = static_cast<MIRJump*>(inst.get());

                            out << "    jmp ." << function.FunctionName << "L" << jump->TargetBlock << "\n";
                        }
                        break;

                    case MIRType::CJMP:
                        {
                            MIRCondJump* jump = static_cast<MIRCondJump*>(inst.get());

                            out << "    ";

                            switch (jump->Cond)
                            {
                                case Condition::EQUAL:
                                    out << "je ";
                                    break;
                                case Condition::NOT_EQUAL:
                                    out << "jne ";
                                    break;
                                case Condition::LESS:
                                    out << "jl ";
                                    break;
                                case Condition::LESS_EQUAL:
                                    out << "jle ";
                                    break;
                                case Condition::GREATER:
                                    out << "jg ";
                                    break;
                                case Condition::GREATER_EQUAL:
                                    out << "jge ";
                                    break;
                            }

                            out << "." << function.FunctionName << "L" << jump->TargetBlock << "\n";
                        }
                        break;

                    case MIRType::CALL:
                        {
                            MIRCall* call = static_cast<MIRCall*>(inst.get());

                            out << "    call " << call->Function << "\n";
                        }
                        break;

                    case MIRType::RET:
                        {
                            out << "    ret\n";
                        }
                        break;

                    case MIRType::CDQ:
                        {
                            out << "    cdq\n";
                        }
                        break;
                }
            }

            out << "\n";
        }

        out << "\n";
    }

    out << ".section .note.GNU-stack, \"\", @progbits\n\n";
}

void PrintASM(std::string AssemblyFilePath)
{
    std::print("------ASSEMBLY-----\n\n");

    std::ifstream AsmFile(AssemblyFilePath, std::ios::in | std::ios::binary | std::ios::ate);

    std::streamsize size = AsmFile.tellg();

    AsmFile.seekg(0, std::ios::beg);

    std::string AsmOutput(size, '\0');

    AsmFile.read(AsmOutput.data(), size);

    std::print("{}\n", AsmOutput);
}


void EmitExecutable(std::string ASMFilePath, std::string ExecFilePath)
{
    std::string cmd = "gcc " + ASMFilePath + " -o " + ExecFilePath + ".out";

    std::system(cmd.c_str());
}

void PrintOutput(std::string ExecFilePath)
{
    pid_t pid = fork();

    if (pid == 0)
    {
        ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);

        execl(("./" + ExecFilePath + ".out").c_str(), ("./" + ExecFilePath + ".out").c_str(), nullptr);

        _exit(1);
    }

    else
    {
        int status;

        waitpid(pid, &status, 0);

        ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);

        struct user_regs_struct regs;

        while (true)
        {
            waitpid(pid, &status, 0);

            if (WIFEXITED(status) || WIFSIGNALED(status))
            {
                break;
            }

            ptrace(PTRACE_GETREGS, pid, nullptr, &regs);

            if (regs.orig_rax == SYS_exit || regs.orig_rax == SYS_exit_group)
            {
                std::print("\033[31mEXIT CODE\033[0m : \033[33m{}\033[0m\n", static_cast<int32_t>(regs.rdi));

                break;
            }

            ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
        }

        ptrace(PTRACE_KILL, pid, nullptr, nullptr);
    }
}
