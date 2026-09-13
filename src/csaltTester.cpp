#include "Globals.h"
#include <charconv>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <print>
#include <string>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <filesystem>
#include <csalt.h>

bool tryParse(std::string str, int& out)
{
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out);

    return ec == std::errc{} && ptr == str.data() + str.size();
}

struct CompileResult
{
    bool compilationTimedOut;
    bool compilationSuccess;

    bool executionSuccess;
    bool executionCrashed;
    bool executionTimedOut;

    int recievedOutput;
};

CompileResult compileTest(std::string flags, std::string filePath)
{
    std::string compileCommand = "timeout 2s csalt " + flags + " " + filePath + " 2>&1";

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(compileCommand.c_str(), "r"), pclose);

    if (!pipe)
    {
        return CompileResult{false, false, false, false, false, -1};
    }

    std::string compileOutput;

    std::string buffer;

    while (fgets(buffer.data(), sizeof(buffer), pipe.get()) != nullptr)
    {
        compileOutput += buffer;
    }

    int status = pclose(pipe.release());

    CompileResult result{};

    result.recievedOutput = -1;

    if (WIFEXITED(status))
    {
        int exitCode = WEXITSTATUS(status);

        if (exitCode == 124)
        {
            result.compilationTimedOut = true;
            result.compilationSuccess = false;
        }

        else
        {
            result.compilationSuccess = (exitCode == 0);
        }
    }

    else
    {
        result.compilationSuccess = false;
    }

    if (compileOutput.find("COMPILED CODE'S EXECUTION TIMED OUT") != std::string::npos)
    {
        result.executionTimedOut = true;
    }

    else if (compileOutput.find("COMPILED CODE'S EXECUTION FAILED") != std::string::npos)
    {
        result.executionCrashed = true;
    }

    else
    {
        size_t pos = compileOutput.find("OUTPUT OF COMPILED CODE");

        if (pos != std::string::npos)
        {
            size_t valPos = compileOutput.find("\033[33m", pos);

            if (valPos != std::string::npos)
            {
                valPos += 5;

                try
                {
                    result.recievedOutput = std::stoi(compileOutput.substr(valPos));
                    result.executionSuccess = true;
                }

                catch (const std::exception&)
                {
                    result.executionCrashed = true;
                }
            }
        }
    }

    return result;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::print("ERROR : tests directory path not given\n");
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

                CompileResult compileResult = compileTest(allFlags, child.path().string());

                if (compileResult.compilationTimedOut)
                {
                    std::print("{:<{}} | {:<{}} | {:<{}} | FLAGS: {}\n", "\033[0;91m[FAIL]\033[0m", TAG_WIDTH, filename, FILE_WIDTH, "Compilation Timed Out", REASON_WIDTH, allFlags);
                    allFlagsPassed = false;
                    return;
                }

                else if (!compileResult.compilationSuccess)
                {
                    std::print("{:<{}} | {:<{}} | {:<{}} | FLAGS: {}\n", "\033[0;91m[FAIL]\033[0m", TAG_WIDTH, filename, FILE_WIDTH, "Compilation Failed", REASON_WIDTH, allFlags);
                    allFlagsPassed = false;
                    return;
                }

                else if (compileResult.executionTimedOut)
                {
                    std::print("{:<{}} | {:<{}} | {:<{}} | FLAGS: {}\n", "\033[0;91m[FAIL]\033[0m", TAG_WIDTH, filename, FILE_WIDTH, "EXECUTION TIMED OUT", REASON_WIDTH, allFlags);
                    allFlagsPassed = false;
                }

                else if (compileResult.executionCrashed)
                {
                    std::print("{:<{}} | {:<{}} | {:<{}} | FLAGS: {}\n", "\033[0;91m[FAIL]\033[0m", TAG_WIDTH, filename, FILE_WIDTH, "SEGFAULT / CRASHED", REASON_WIDTH, allFlags);
                    allFlagsPassed = false;
                }

                else if (compileResult.executionSuccess && !(compileResult.recievedOutput == expectedOutput))
                {
                    std::string reason = std::format("EXPECTED: {}, RECIEVED: {}", expectedOutput, compileResult.recievedOutput);
                    std::print("{:<{}} | {:<{}} | {:<{}} | FLAGS: {}\n", "\033[0;91m[FAIL]\033[0m", TAG_WIDTH, filename, FILE_WIDTH, reason, REASON_WIDTH, allFlags);
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
