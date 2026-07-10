#include "PassManager.h"

void PassManager::RunOptimizations(TAC& TAC)
{
    for (PassGroup& passGroup : OptimizationPipeline)
    {
        if (passGroup.IterateToFixedPoint)
        {
            bool changed = true;

            while (changed)
            {
                for (Pass& pass : passGroup.Passes)
                {
                    changed |= pass.Run(TAC);
                }
            }
        }
    }
}
