#include "PassManager.h"

void PassManager::RunOptimizations(TAC* TAC, MIR* MIR)
{
    for (PassGroup& passGroup : OptimizationPipeline)
    {
        if (passGroup.IterateToFixedPoint)
        {
            bool changed = true;

            while (changed)
            {
                changed = false;

                for (Pass& pass : passGroup.Passes)
                {
                    if (pass.Enabled)
                    {
                        if (TAC != nullptr)
                        {
                            changed |= pass.Func.RunTACIter(*TAC);
                        }

                        else
                        {
                            changed |= pass.Func.RunMIRIter(*MIR);
                        }
                    }
                }
            }
        }

        else
        {
            for (Pass& pass : passGroup.Passes)
            {
                if (pass.Enabled)
                {
                    if (TAC != nullptr)
                    {
                        pass.Func.RunTAC(*TAC);
                    }

                    else
                    {
                        pass.Func.RunMIR(*MIR);
                    }
                }
            }
        }
    }
}
