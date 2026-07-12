#include <string>
#include <vector>

template<typename T> union PassFuncUnion
{
    void (*Run)(T&);
    bool (*RunIter)(T&);
};

template<typename T> struct Pass
{
    std::string Name;

    bool Enabled = true;

    bool PrintDiff = false;

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

    template<typename R> void RunOptimizations(R& IR)
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
                        if (pass.Enabled)
                        {
                            changed |= pass.Func.RunIter(IR);
                        }
                    }
                }
            }

            else
            {
                for (Pass pass : passGroup.Passes)
                {
                    if (pass.Enabled)
                    {
                        pass.Func.Run(IR);
                    }
                }
            }
        }
    };
};
