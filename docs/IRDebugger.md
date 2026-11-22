# IRDebugger

The IRDebugger class handles printing compiler IR during different compilation phases.

It stores references to the compiler's intermediate representations and uses phase metadata to determine which representation to print.

The `AddIR()` function registers the TokenStream, AST, CFG, TAC, MIR, and assembly file path. These are later used by `Notify()`, which relies on phase metadata to determine which representation to print.

My original intention was to have the debugger automatically verify IRs after every compilation phase and much more, hence the name `IRDebugger` instead of `IRPrinter` or something. I didn't get around to implementing the verification functions, so the class currently only handles debugging dumps.
