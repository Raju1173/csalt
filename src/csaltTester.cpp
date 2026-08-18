#include "Globals.h"
#include <charconv>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <print>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>
#include <filesystem>
#include <csalt.h>

bool tryParse(std::string str, int& out)
{
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out);

    return ec == std::errc{} && ptr == str.data() + str.size();
}

bool compileTest(std::string flags, std::string filePath)
{
    std::string compileCommand = "csalt " + flags + " " + filePath + " > /dev/null 2>&1";

    int status = std::system(compileCommand.c_str());

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

struct TestResult
{
    bool success;
    bool crashed;
    int actualOutput;
};

TestResult compareTestOutput(std::string ExecFilePath, int expected)
{
    std::string runPath = ExecFilePath;

    if (runPath.find('/') == std::string::npos)
    {
        runPath = "./" + runPath;
    }

    pid_t pid = fork();

    if (pid == 0)
    {
        ptrace(PTRACE_TRACEME, 0, nullptr, nullptr);

        execl(runPath.c_str(), runPath.c_str(), nullptr);

        _exit(1);
    }

    else
    {
        int status;

        waitpid(pid, &status, 0);

        ptrace(PTRACE_SETOPTIONS, pid, nullptr, PTRACE_O_EXITKILL | PTRACE_O_TRACESYSGOOD);

        struct user_regs_struct regs;

        bool crashed = false;
        int actualOutput;

        while (true)
        {
            ptrace(PTRACE_SYSCALL, pid, nullptr, nullptr);
            waitpid(pid, &status, 0);

            if (WIFEXITED(status))
            {
                actualOutput = WEXITSTATUS(status);
                break;
            }

            if (WIFSIGNALED(status))
            {
                crashed = true;
                break;
            }

            if (WIFSTOPPED(status))
            {
                int sig = WSTOPSIG(status);

                if (sig != (SIGTRAP | 0x80))
                {
                    if (sig == SIGSEGV || sig == SIGILL || sig == SIGABRT || sig == SIGFPE)
                    {
                        crashed = true;
                        break;
                    }

                    ptrace(PTRACE_SYSCALL, pid, nullptr, sig);
                    continue;
                }

                ptrace(PTRACE_GETREGS, pid, nullptr, &regs);

                if (regs.orig_rax == SYS_exit || regs.orig_rax == SYS_exit_group)
                {
                    actualOutput = static_cast<int32_t>(regs.rdi);
                    break;
                }
            }
        }

        ptrace(PTRACE_KILL, pid, nullptr, nullptr);
        waitpid(pid, &status, 0);

        return {!crashed && (actualOutput == expected), crashed, actualOutput};
    }
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::print("Error : tests directory path not given\n");
        return 1;
    }

    std::string permanentFlags = "";

    for (int i = 1; i < argc - 1; i++)
    {
        permanentFlags += " " + std::string(argv[i]);
    }

    const int TAG_WIDTH = 6;
    const int FILE_WIDTH = 35;
    const int REASON_WIDTH = 40;

    for (auto& child : std::filesystem::recursive_directory_iterator(std::string(argv[argc - 1])))
    {
        if (child.is_regular_file() && child.path().extension() == ".c")
        {
            std::string filename = child.path().filename().string();
            std::ifstream file(child.path());

            if (!file.is_open())
            {
                std::print("{:<{}} | {:<{}} | {}\n", "\033[0;93m[ERR]\033[0m", TAG_WIDTH, filename, FILE_WIDTH, "Could not open file");
                continue;
            }

            std::string line;
            bool found = false;

            while (std::getline(file, line))
            {
                if (!line.empty())
                {
                    found = true;
                    break;
                }
            }

            int expectedOutput;

            if (!found || !line.starts_with("// EXPECTED : ") || !tryParse(line.substr(14), expectedOutput))
            {
                std::print("{:<{}} | {:<{}} | {}\n", "\033[0;93m[ERR]\033[0m", TAG_WIDTH, filename, FILE_WIDTH, "Missing or invalid '// EXPECTED : [out]' header");
                continue;
            }

            std::string ExecutableFilePath = std::string(child.path(), 0, std::strlen(child.path().c_str()) - 1) + "out";

            bool allFlagsPassed = true;

            auto runTestCase = [&](std::string flags) {
                std::string allFlags = flags + permanentFlags;

                if (!compileTest(allFlags, child.path().string()))
                {
                    std::print("{:<{}} | {:<{}} | {:<{}} | Flags: {}\n", "\033[0;91m[FAIL]\033[0m", TAG_WIDTH, filename, FILE_WIDTH, "Compilation Failed", REASON_WIDTH, allFlags);
                    allFlagsPassed = false;
                    return;
                }

                TestResult testResult = compareTestOutput(ExecutableFilePath, expectedOutput);

                if (testResult.crashed)
                {
                    std::print("{:<{}} | {:<{}} | {:<{}} | Flags: {}\n", "\033[0;91m[FAIL]\033[0m", TAG_WIDTH, filename, FILE_WIDTH, "SEGFAULT / CRASHED", REASON_WIDTH, allFlags);
                    allFlagsPassed = false;
                }

                else if (!testResult.success)
                {
                    std::string reason = std::format("Expected: {}, Recieved: {}", expectedOutput, testResult.actualOutput);
                    std::print("{:<{}} | {:<{}} | {:<{}} | Flags: {}\n", "\033[0;91m[FAIL]\033[0m", TAG_WIDTH, filename, FILE_WIDTH, reason, REASON_WIDTH, allFlags);
                    allFlagsPassed = false;
                }
            };

            runTestCase("");

            runTestCase("--disable-all");

            for (auto& [target, phase] : targetPhaseMap)
            {
                if (getPhaseMetadata(phase).isOptimizationPhase)
                {
                    runTestCase("--disable-" + target);
                    runTestCase("--disable-all --enable-" + target);
                }
            }

            if (allFlagsPassed)
            {
                std::print("{:<{}} | {:<{}} | {}\n", "\033[0;92m[PASS]\033[0m", TAG_WIDTH, filename, FILE_WIDTH, "All tests passed");
            }
        }
    }

    return 0;
}
