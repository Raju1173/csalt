# IR History Tracking

Probably the most overengineered part of this compiler...

The IREditor maintains a history of transformations performed on TAC and MIR.

Most transformation operations accept an optional `Message` containing the compilation pass, the type of transformation, and additional information about the change.

### Transformation Messages

Each `Message` contains three fields:

- `Pass` : The compilation phase that performed the transformation.
- `TranformationType` : The type of transformation performed.
- `Info` : Additional information describing the change.

The supported transformation types are:

- `ADDED`
- `DELETED`
- `MOVED`
- `CLONED`
- `REPLACED`
- `MODIFIED`

Messages can be attached to any IR object (instructions, blocks, functions).

History tracking is optional. Passes can provide messages when they want to document a transformation and omit them otherwise.

Each message is printed as :

`[Transformation Type] BY [Compilation Phase] : Information about the transformation`

### Deleted IR Preservation

The BIGGEST problem with tracking transformation history is that deleted IR objects would normally be destroyed, taking their history with them.

Leaving deleted objects in their original containers with a simple `deleted` flag would make it extremely clunky to traverse through the IR since we'd need to make sure that we're skipping deleted objects every single time...

So to avoid this, I introduced `DeadSiblings` (IRCommon.h).

Each IR object can retain deleted siblings in two lists: `preceding` and `trailing`.

When an object is deleted, `cascadeDeletion()` (IREditor.h) transfers its ownership, along with any previously retained dead siblings, to a neighboring surviving object. Their relative ordering is preserved.

If no siblings remain, that object goes or cascades into it's parent IR object's `LastDead[Object]` field.

This allows deleted IR objects and their transformation histories to remain accessible even after they are removed from their original containers.
