# CSalt
An optimization focused compiler for a minimal subset of C targeting x86-64 assembly

# Performance Benchmarks

- **Benchmark command** : `hyperfine -i -N ./[fileName]`
- **CPU**: Intel i5-13420H (13th Gen)
- **Clang Version**: 18.1.3
- **GCC Version**: 13.3.0

| Type Of Benchmarks | GCC -O1 | Clang -O1 | CSalt |
|---|---|---|---|
|  | 100% () |  |  |
|  | 100% () |  |  |
|  | 100% () |  |  |
|  | 100% () |  |  |

# Optimization Passes

1. Function Inlining
2. Compile Time Function Execution (disabled by default)
3. Tail Recursion Elimination
4. Sibling Call Optimization
5. Loop Invariant Code Motion
6. Induction Variable Merging
7. Loop Inversion
8. Global Value Numbering
9. Algebraic Simplification
10. Constant Folding
11. If Conversion
12. Branch Simplification
13. Control Flow Simplification
14. Dead Code Elimination
15. Frame Pointer Omission

# Language Features

CSalt only supports 6 features of C :

- int
- if
- while
- local variables
- functions
- return

Keeping the fronted super small *(like a grain of **sea salt**)* makes implementing complex optimizations extremely simple and some even become almost trivial

# Unsupported Features

- else blocks *(important, use consequent if blocks)*
- any bitwise or logical operator *(important)*
- Variable shadowing *(important)*
- Any unary operator except '-'
- Structs
- Unions
- Arrays
- Pointers
- Preprocessor statements
- Other fancy C features like "int x, y;", etc.
- Any other keyword except int, if, while or return

# Architecture

`Source Code -> Tokens -> AST -> CFG -> TAC -> Pre SSA Optimizations -> SSA TAC -> Post SSA Optimizations -> Machine IR -> Backend Optimizations -> Assembly`

*Note : I'm storing AST nodes inside the CFG because it felt much simpler than converting from AST to TAC directly...*

# Usage

### CSalt can only compile a single file with no dependencies :

`csalt fileName`

### Every IR can be inspected at every single compilation step individually using flags :

```
csalt   --dump-tok

        --dump-ast

        --dump-cfg

        --dump-tac-const
        --dump-tac-phi-ins
        --dump-tac-rename
        --dump-tac-opt
        --dump-tac-phi-res

        --dump-mir-const
        --dump-mir-reg-alloc
        --dump-mir-opt

        --dump-asm

        [fileName].c
```

### Any optimization pass can be enabled and disabled using :

```
csalt   --disable/enable-folding
        --disable/enable-alg-simp
        --disable/enable-brn-simp
        --disable/enable-cfg-simp
        --disable/enable-dce
        --disable/enable-gvn
        --disable/enable-sco
        --disable/enable-licm
        --disable/enable-ctfe (disabled by default)

        --disable/enable-reg-alloc
        --disable/enable-fpo

        [fileName].c
```
