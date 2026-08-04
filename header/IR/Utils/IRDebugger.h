#pragma once

#include "Globals.h"
#include "IRFormatters.h"
#include <algorithm>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <vector>

struct IRSnapshot
{
    std::string Name;
    std::string Snapshot;
};

class Degubber
{
private:
    inline static std::vector<IRSnapshot> StaticIRSnapshots;
    inline static std::vector<IRSnapshot> InteractiveIRSnapshots;

public:
    template<typename R> static void TakeSnapshot(std::string SnapshotName, R& IR, PhaseOptions phaseOptions)
    {
        if (phaseOptions.Dump)
            StaticIRSnapshots.push_back(IRSnapshot{SnapshotName, FormatIR(IR)});
        if (phaseOptions.HistoryDump)
            StaticIRSnapshots.push_back(IRSnapshot{SnapshotName, FormatIR(IR, true)});
        if (phaseOptions.InteractiveDump)
            InteractiveIRSnapshots.push_back(IRSnapshot{SnapshotName, FormatIR(IR)});
        if (phaseOptions.InteractiveHistoryDump)
            InteractiveIRSnapshots.push_back(IRSnapshot{SnapshotName, FormatIR(IR, true)});
    };

    static void Run()
    {
        if (!InteractiveIRSnapshots.empty())
        {
            int i = 0;

            std::string prevInput = "n";

            while (true)
            {
                std::print("\033[2J\033[3J\033[1;1H");
                std::print("-----{}-----\n\n", InteractiveIRSnapshots[i].Name);
                std::print("{}", InteractiveIRSnapshots[i].Snapshot);
                std::print("\nSnapshot : {}/{}\n", i + 1, InteractiveIRSnapshots.size());
                std::print("\nControls : [n/p [N]] - next/prev N times, [q] - quit, [Enter] - repeat\n\n");
                std::print("(csalt) ");

                std::string input;
                std::getline(std::cin, input);

                if (input.empty())
                    input = prevInput;
                else
                    prevInput = input;

                std::istringstream iss(input);
                std::string cmd;
                int steps = 1;

                iss >> cmd;
                if (iss >> steps)
                {
                    if (steps < 0)
                        steps = 0;
                }

                if (cmd == "n" || cmd == "next")
                {
                    i = std::min(i + steps, static_cast<int>(InteractiveIRSnapshots.size()) - 1);
                }

                else if (cmd == "p" || cmd == "prev")
                {
                    i = std::max(i - steps, 0);
                }

                else if (cmd == "q" || cmd == "quit")
                {
                    std::print("\033[2J\033[3J\033[1;1H");
                    break;
                }
            }
        }

        for (IRSnapshot Snapshot : StaticIRSnapshots)
        {
            std::print("-----{}-----\n\n", Snapshot.Name);
            std::print("{}", Snapshot.Snapshot);
        }
    };

    static void ClearSnapshots()
    {
        StaticIRSnapshots.clear();
        InteractiveIRSnapshots.clear();
    };
};
