#pragma once

#include <string>
#include <vector>

struct IRSnapshot
{
    std::string Name;
    std::string Snapshot;
};

inline std::vector<IRSnapshot> IRSnapshots;

template<typename R> inline void TakeSnapshot(std::string SnapshotName, R& IR, bool history = false)
{
    IRSnapshots.push_back(IRSnapshot{SnapshotName});
};

class Degubber
{
    //
};
