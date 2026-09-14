#include "Globals.h"
#include "IRDebugger.h"
#include "MIRGenerator.h"
#include "MIRInstructions.h"
#include <fstream>
#include <print>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/resource.h>
#include <thread>

void EmitAssembly(MIR& MIR, const std::string& filename)
{
    Debugger::AddIR(filename);

    std::ofstream out(filename);

    out << ".intel_syntax noprefix\n\n";

    out << ".text\n\n";

    for (auto& function : MIR)
    {
        if (function->FunctionName == "main")
            out << ".global main\n";

        out << function->FunctionName << ":\n";

        for (const auto& block : function->Blocks)
        {
            out << "." << function->FunctionName << "L" << block->ID << ":\n";

            for (const auto& inst : block->Instructions)
            {
                switch (inst->type)
                {
                    case MIRInstType::MOV:
                        {
                            MIRMov* mov = static_cast<MIRMov*>(inst.get());

                            out << "    mov " << OperandString(mov->Dest) << ", " << OperandString(mov->Source) << "\n";
                        }
                        break;

                    case MIRInstType::MOVZX:
                        {
                            MIRMovzx* movzx = static_cast<MIRMovzx*>(inst.get());

                            out << "    movzx " << OperandString(movzx->Dest) << ", " << OperandString(movzx->Source) << "\n";
                        }
                        break;

                    case MIRInstType::CMOV:
                        {
                            MIRCmov* cmov = static_cast<MIRCmov*>(inst.get());

                            out << "    ";

                            switch (cmov->Cond)
                            {
                                case Condition::EQUAL:
                                    out << "cmove ";
                                    break;
                                case Condition::NOT_EQUAL:
                                    out << "cmovne ";
                                    break;
                                case Condition::LESS:
                                    out << "cmovl ";
                                    break;
                                case Condition::LESS_EQUAL:
                                    out << "cmovle ";
                                    break;
                                case Condition::GREATER:
                                    out << "cmovg ";
                                    break;
                                case Condition::GREATER_EQUAL:
                                    out << "cmovge ";
                                    break;
                            }

                            out << OperandString(cmov->Dest) << "," << OperandString(cmov->Source) << "\n";
                        }
                        break;

                    case MIRInstType::ADD:
                        {
                            MIRAdd* add = static_cast<MIRAdd*>(inst.get());

                            out << "    add " << OperandString(add->Dest) << ", " << OperandString(add->Source) << "\n";
                        }
                        break;

                    case MIRInstType::SUB:
                        {
                            MIRSub* sub = static_cast<MIRSub*>(inst.get());

                            out << "    sub " << OperandString(sub->Dest) << ", " << OperandString(sub->Source) << "\n";
                        }
                        break;

                    case MIRInstType::MUL:
                        {
                            MIRImul* mul = static_cast<MIRImul*>(inst.get());

                            out << "    imul " << OperandString(mul->Dest) << ", " << OperandString(mul->Source) << "\n";
                        }
                        break;

                    case MIRInstType::DIV:
                        {
                            MIRIdiv* div = static_cast<MIRIdiv*>(inst.get());

                            out << "    idiv " << OperandString(div->Divisor) << "\n";
                        }
                        break;

                    case MIRInstType::NEG:
                        {
                            MIRNeg* neg = static_cast<MIRNeg*>(inst.get());

                            out << "    neg " << OperandString(neg->Dest) << "\n";
                        }
                        break;

                    case MIRInstType::XOR:
                        {
                            MIRXor* xorInst = static_cast<MIRXor*>(inst.get());

                            out << "    xor " << OperandString(xorInst->Dest) << ", " << OperandString(xorInst->Source) << "\n";
                        }
                        break;

                    case MIRInstType::CMP:
                        {
                            MIRCmp* cmp = static_cast<MIRCmp*>(inst.get());

                            out << "    cmp " << OperandString(cmp->Left) << ", " << OperandString(cmp->Right) << "\n";
                        }
                        break;

                    case MIRInstType::TEST:
                        {
                            MIRTest* test = static_cast<MIRTest*>(inst.get());

                            out << "    test " << OperandString(test->Left) << ", " << OperandString(test->Right) << "\n";
                        }
                        break;

                    case MIRInstType::PUSH:
                        {
                            MIRPush* push = static_cast<MIRPush*>(inst.get());

                            out << "    push " << OperandString(push->Source) << "\n";
                        }
                        break;

                    case MIRInstType::POP:
                        {
                            MIRPop* pop = static_cast<MIRPop*>(inst.get());

                            out << "    pop " << OperandString(pop->Dest) << "\n";
                        }
                        break;

                    case MIRInstType::JMP:
                        {
                            MIRJump* jump = static_cast<MIRJump*>(inst.get());

                            if (std::holds_alternative<MIRBlock*>(jump->Target))
                                out << "    jmp ." << function->FunctionName << "L" << std::get<MIRBlock*>(jump->Target)->ID << "\n";
                            else if (std::holds_alternative<MIRFunction*>(jump->Target))
                                out << "    jmp " << std::get<MIRFunction*>(jump->Target)->FunctionName << "\n";
                        }
                        break;

                    case MIRInstType::CJMP:
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

                            out << "." << function->FunctionName << "L" << jump->TargetBlock->ID << "\n";
                        }
                        break;

                    case MIRInstType::CALL:
                        {
                            MIRCall* call = static_cast<MIRCall*>(inst.get());

                            out << "    call " << call->Function->FunctionName << "\n";
                        }
                        break;

                    case MIRInstType::SET:
                        {
                            MIRSet* set = static_cast<MIRSet*>(inst.get());

                            out << "    ";

                            switch (set->Cond)
                            {
                                case Condition::EQUAL:
                                    out << "sete ";
                                    break;
                                case Condition::NOT_EQUAL:
                                    out << "setne ";
                                    break;
                                case Condition::LESS:
                                    out << "setl ";
                                    break;
                                case Condition::LESS_EQUAL:
                                    out << "setle ";
                                    break;
                                case Condition::GREATER:
                                    out << "setg ";
                                    break;
                                case Condition::GREATER_EQUAL:
                                    out << "setge ";
                                    break;
                            }

                            out << OperandString(set->Dest) << "\n";
                        }
                        break;

                    case MIRInstType::RET:
                        {
                            out << "    ret\n";
                        }
                        break;

                    case MIRInstType::CDQ:
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

    Debugger::Notify(Phase::ASM);
}

// using gcc to assemble and link instead of writing a simple startup sequence to keep benchmarks fair...

void EmitExecutable(std::string ASMFilePath, std::string ExecFilePath)
{
    std::string cmd = "gcc " + ASMFilePath + " -o " + ExecFilePath;

    std::system(cmd.c_str());
}

void PrintOutput(std::string ExecFilePath)
{
    if (gCompilerOptions[Phase::OUTPUT].enabled == false)
        return;

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
        auto startTime = std::chrono::steady_clock::now();

        auto waitWithTimeout = [&startTime, &pid, &status]() -> bool {
            while (true)
            {
                int res = waitpid(pid, &status, WNOHANG);

                if (res > 0)
                    return true;
                if (res == -1)
                    return false;

                auto now = std::chrono::steady_clock::now();

                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() >= 1000)
                {
                    return false;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        };

        if (!waitWithTimeout())
        {
            kill(pid, SIGKILL);

            waitpid(pid, &status, 0);

            std::print("\033[35mCOMPILED CODE'S EXECUTION TIMED OUT\033[0m\n");

            return;
        }

        ptrace(PTRACE_SETOPTIONS, pid, nullptr, PTRACE_O_EXITKILL | PTRACE_O_TRACESYSGOOD);

        struct user_regs_struct regs;

        bool timedOut = false;

        while (true)
        {
            ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);

            if (!waitWithTimeout())
            {
                timedOut = true;
                break;
            }

            if (WIFEXITED(status))
            {
                std::print("\033[35mOUTPUT OF COMPILED CODE\033[0m : \033[33m{}\033[0m\n", WIFEXITED(status));
                break;
            }

            if (WIFSIGNALED(status))
            {
                std::print("\033[35mCOMPILED CODE'S EXECUTION FAILED\033[0m\n");
                break;
            }

            if (WIFSTOPPED(status))
            {
                int sig = WSTOPSIG(status);

                if (sig != (SIGTRAP | 0x80))
                {
                    if (sig == SIGSEGV || sig == SIGILL || sig == SIGABRT || sig == SIGFPE)
                    {
                        std::print("\033[35mCOMPILED CODE'S EXECUTION FAILED\033[0m\n");
                        break;
                    }

                    ptrace(PTRACE_SYSCALL, pid, nullptr, sig);

                    continue;
                }

                ptrace(PTRACE_GETREGS, pid, nullptr, &regs);

                if (regs.orig_rax == SYS_exit || regs.orig_rax == SYS_exit_group)
                {
                    std::print("\033[35mOUTPUT OF COMPILED CODE\033[0m : \033[33m{}\033[0m\n", static_cast<int32_t>(regs.rdi));
                    break;
                }
            }
        }

        if (timedOut)
        {
            kill(pid, SIGKILL);

            waitpid(pid, &status, 0);

            std::print("\033[35mCOMPILED CODE'S EXECUTION TIMED OUT\033[0m\n");
        }

        ptrace(PTRACE_KILL, pid, nullptr, nullptr);

        waitpid(pid, &status, 0);
    }
}
