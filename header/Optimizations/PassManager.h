#include "Globals.h"
#include "IRDebugger.h"
#include "MIRGenerator.h"
#include "TACGenerator.h"
#include <optional>
#include <string>
#include <vector>

template<typename T> union PassFuncUnion
{
    void (*Run)(T&);
    bool (*RunIter)(T&);
};

template<typename T> struct Pass
{
    Phase OptimizationPass;

    PassFuncUnion<T> Func;
};

template<typename T> struct PassGroup
{
    bool IterateToFixedPoint = false;
    std::vector<Pass<T>> Passes;
};

template<typename T> class PassManager
{
public:
    std::vector<PassGroup<T>> OptimizationPipeline;

    PassManager(std::vector<PassGroup<T>> OptimizationPipeline) : OptimizationPipeline(OptimizationPipeline){};

    template<typename R> void RunOptimizations(R& IR, std::optional<Phase> phase)
    {
        for (PassGroup passGroup : OptimizationPipeline)
        {
            if (passGroup.IterateToFixedPoint)
            {
                bool changed = true;

                while (changed)
                {
                    changed = false;

                    for (Pass pass : passGroup.Passes)
                    {
                        if (gCompilerOptions[pass.OptimizationPass].enabled)
                        {
                            changed |= pass.Func.RunIter(IR);
                        }
                    }
                }

                for (Pass pass : passGroup.Passes)
                {
                    if (gCompilerOptions[pass.OptimizationPass].enabled)
                    {
                        Debugger::Notify(pass.OptimizationPass);
                        break;
                    }
                }
            }

            else
            {
                for (Pass pass : passGroup.Passes)
                {
                    if (gCompilerOptions[pass.OptimizationPass].enabled)
                    {
                        pass.Func.Run(IR);

                        Debugger::Notify(pass.OptimizationPass);
                    }
                }
            }
        }

        if (phase.has_value())
            Debugger::Notify(phase.value());
    };
};
