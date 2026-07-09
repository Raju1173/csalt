#include "MIRGenerator.h"
#include <fstream>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/resource.h>

static std::string RegisterName(Register reg)
{
    switch (reg)
    {
        case Register::EAX:
            return "eax";

        case Register::EDI:
            return "edi";
        case Register::ESI:
            return "esi";
        case Register::EDX:
            return "edx";
        case Register::ECX:
            return "ecx";
        case Register::R8D:
            return "r8d";
        case Register::R9D:
            return "r9d";

        case Register::R10D:
            return "r10d";
        case Register::R11D:
            return "r11d";

        case Register::EBX:
            return "ebx";
        case Register::R12D:
            return "r12d";
        case Register::R13D:
            return "r13d";
        case Register::R14D:
            return "r14d";
        case Register::R15D:
            return "r15d";

        case Register::RSP:
            return "rsp";
        case Register::RBP:
            return "rbp";
    }

    return "";
}

static std::string OperandString(const Operand& op)
{
    return std::visit([](auto&& value) -> std::string {
        using T = std::decay_t<decltype(value)>;

        if constexpr (std::is_same_v<T, Register>)
        {
            return RegisterName(value);
        }

        else if constexpr (std::is_same_v<T, StackOffset>)
        {
            if (value.Offset < 0)
                return "DWORD PTR [rbp" + std::to_string(value.Offset) + "]";

            if (value.Offset > 0)
                return "DWORD PTR [rbp + " + std::to_string(value.Offset) + "]";

            return "DWORD PTR [rbp]";
        }

        else
        {
            return std::to_string(value.Value);
        }
    },
        op);
}

void EmitAssembly(MIR& MIR, const std::string& filename)
{
    std::ofstream out(filename);

    out << ".intel_syntax noprefix\n";
    out << ".text\n\n";

    for (const auto& function : MIR)
    {
        if (function.FunctionName == "main")
            out << ".global main\n";

        out << function.FunctionName << ":\n";

        for (const auto& block : function.Blocks)
        {
            out << "." << function.FunctionName << "L" << block.ID << ":\n";

            for (const auto& inst : block.Instructions)
            {
                if (auto mov = dynamic_cast<MIRMov*>(inst.get()))
                {
                    out << "    mov " << OperandString(mov->Dest) << ", " << OperandString(mov->Source) << "\n";
                }

                else if (auto add = dynamic_cast<MIRAdd*>(inst.get()))
                {
                    out << "    add " << OperandString(add->Dest) << ", " << OperandString(add->Source) << "\n";
                }

                else if (auto sub = dynamic_cast<MIRSub*>(inst.get()))
                {
                    out << "    sub " << OperandString(sub->Dest) << ", " << OperandString(sub->Source) << "\n";
                }

                else if (auto mul = dynamic_cast<MIRImul*>(inst.get()))
                {
                    out << "    imul " << OperandString(mul->Dest) << ", " << OperandString(mul->Source) << "\n";
                }

                else if (auto div = dynamic_cast<MIRIdiv*>(inst.get()))
                {
                    out << "    idiv " << OperandString(div->Divisor) << "\n";
                }

                else if (auto neg = dynamic_cast<MIRNeg*>(inst.get()))
                {
                    out << "    neg " << OperandString(neg->Dest) << "\n";
                }

                else if (auto cmp = dynamic_cast<MIRCmp*>(inst.get()))
                {
                    out << "    cmp " << OperandString(cmp->Left) << ", " << OperandString(cmp->Right) << "\n";
                }

                else if (auto push = dynamic_cast<MIRPush*>(inst.get()))
                {
                    out << "    push " << OperandString(push->Source) << "\n";
                }

                else if (auto pop = dynamic_cast<MIRPop*>(inst.get()))
                {
                    out << "    pop " << OperandString(pop->Dest) << "\n";
                }

                else if (auto jump = dynamic_cast<MIRJump*>(inst.get()))
                {
                    out << "    jmp ." << function.FunctionName << "L" << jump->TargetBlock << "\n";
                }

                else if (auto jump = dynamic_cast<MIRCondJump*>(inst.get()))
                {
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

                else if (auto call = dynamic_cast<MIRCall*>(inst.get()))
                {
                    out << "    call " << call->Function << "\n";
                }

                else if (dynamic_cast<MIRRet*>(inst.get()))
                {
                    out << "    ret\n";
                }

                else if (dynamic_cast<MIRCdq*>(inst.get()))
                {
                    out << "    cdq\n";
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
    //
}


void EmitExecutable(std::string ASMFilePath, std::string ExecFilePath)
{
    std::string cmd = "gcc " + ASMFilePath + " -o " + ExecFilePath;

    std::system(cmd.c_str());
}

void PrintOutput(std::string ExecFilePath)
{
    pid_t pid = fork();

    if (pid == 0)
    {
        ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);

        execl(("./" + ExecFilePath).c_str(), ("./" + ExecFilePath).c_str(), nullptr);

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
