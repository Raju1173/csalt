#pragma once

#include "TACGenerator.h"

void InsertPhiNodes(TAC& TAC);

void RenameVariables(TAC& TAC);

void SplitCriticalEdges(TAC& TAC);

void ResolvePhiNodes(TAC& TAC);
