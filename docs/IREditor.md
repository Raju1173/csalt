# IREditor

The IREditor class is a generalised transformation API for TAC and MIR.

It's the centralized interface for modifying IR, automatic analysis invalidation, and transformation history tracking.

## Responsibilities

### IR Transformation

All IR transformations are expected to pass through the IREditor.

The editor has functions for modifying instructions, blocks, control flow edges, and functions.

Passes can combine these operations to perform more complex transformations in atomic steps which makes the code easier to follow.

### Analysis Invalidation

IR analyses depend on being notified when their results become invalid so they can recompute themselves the next time they are accessed.

Each analysis maintains an 'isValid' flag. When a transformation affects an analysis, the editor sets the corresponding flag to false.

The invalidation is handled according to the type of transformation :-

- Data flow changes : Invalidate analyses such as variable uses, definition blocks, and induction variables.
- Control flow changes : Invalidate analyses such as dominators, dominance frontiers, and loop information.
- Structural changes : May require invalidating all analyses.

This removes the need for individual passes to manage analysis invalidation themselves for the most part...

### Transformation History

Most transformation operations accept an optional message argument.

When provided, the message is added to the history of the affected IR object. This allows passes to leave a record of the changes they perform.

History tracking is optional and can be omitted when no history entry is needed but is extremely useful for debugging.

## BIG PROBLEM

Individual variable mutations are currently an exception to the centralized mutation thing.

Variable fields are directly accessible, so modifications to them will bypass the editor.

Thus, such modifications will not trigger analysis invalidation or transformation history tracking. Passes are responsible for maintaining analysis consistency when modifying variables directly...

I just could not solve this problem in a clean way... writing a generalised wrapper for variable reassignments like `IREditor<IRTypes>::assignValue()` was an option but it looked horrible to read and would've probably made debugging much harder than it already is...
