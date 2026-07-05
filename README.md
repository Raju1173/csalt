# CSalt
An optimization focused compiler for a minimal turing complete subset of C, targeting x86-64 assembly

# Performance Benchmarks

| Compiler |  |
|---|---|
| GCC -O1 |  |
| GCC -O0 |  |
| CSalt |  |

# Language Features

CSalt only supports 6 features of C :

- int
- if
- while
- local variables
- functions
- return

This super small frontend makes implementing optimizations much easier

# Architecture

`Source Code -> Lexer -> Parser -> CFG Construction -> SSA Construction -> TAC Generation -> Machine IR -> Assembly`

# Usage

### It can only compile a single file with no dependencies :

`csalt fileName`

### You can inspect the output of every single compilation step individually using flags :

`csalt dump-tok dump-ast dump-cfg dump-tac dump-asm fileName`\
or\
`csalt dump-all fileName`
