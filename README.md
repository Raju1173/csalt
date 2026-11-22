# CSalt
An optimization focused compiler for a minimal subset of C

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
7. Loop Strength Reduction
8. Induction Variable Elimination
9. Loop Inversion
10. Global Value Numbering
11. Algebraic Simplification
12. Constant Folding
13. If Conversion
14. Branch Simplification
15. Control Flow Simplification
16. Dead Code Elimination
17. Frame Pointer Omission
18. Phi Simplification
19. Compare Elimination
20. Register Copy Propagation
21. Peephole Optimizations
22. Graph Coloring Register Allocation

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
- Function declarations *(important)*
- Variable shadowing *(important)*
- Any unary operator except '-'
- Structs
- Unions
- Arrays
- Pointers
- Preprocessor statements
- Other fancy C features like "int x, y;", etc.
- Any other keyword except int, if, while or return

# Compilation Pipeline

`Source Code -> Tokens -> AST -> CFG -> TAC -> Pre SSA Optimizations -> SSA TAC -> Post SSA Optimizations -> Machine IR -> Backend Optimizations -> Assembly`

*Note : I'm storing AST nodes inside the CFG because it felt much simpler than converting from AST to TAC directly...*

# Usage

`caslt --[flag1] --[flag2] ... --[flagN] [fileName].c`

There is no strict order for passing the flags but they get processed sequentially so if you enable something in an earlier flag and disable it in a later flag, it will end up disabled and vice versa...

*Note : CSalt can only compile a single C file with no dependencies...*

### Every IR can be inspected at every single compilation step individually using flags :

```
csalt   --dump-tok

        --dump-ast

        --dump-cfg

        --dump-tac-const
        --dump-tac-pre-ssa-opt
        --dump-tac-phi-ins
        --dump-tac-rename
        --dump-tac-opt
        --dump-tac-post-ssa-opt
        --dump-tac-edge-split
        --dump-tac-phi-res
        --dump-tac (after all transformations to TAC have been made)

        --dump-mir-const
        --dump-tac-pre-ra-opt
        --dump-mir-reg-alloc
        --dump-mir-post-ra-opt
        --dump-mir (after all transformations to MIR have been made)

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
        --disable/enable-inlining
        --disable/enable-if-conv
        --disable/enable-ive
        --disable/enable-ivm
        --disable/enable-loop-inv
        --disable/enable-lsr
        --disable/enable-phi-simp
        --disable/enable-sco
        --disable/enable-tre

        --disable/enable-reg-alloc
        --disable/enable-fpo
        --disable/enable-cmp-elim
        --disable/enable-reg-cprop
        --disable/enable-peep-opt

        [fileName].c
```

### The immediate output of any optimization pass can be printed using :

```
csalt   --dump-folding
        --dump-alg-simp
        --dump-brn-simp
        --dump-cfg-simp
        --dump-dce
        --dump-gvn
        --dump-sco
        --dump-licm
        --dump-ctfe (disabled by default)
        --dump-inlining
        --dump-if-conv
        --dump-ive
        --dump-ivm
        --dump-loop-inv
        --dump-lsr
        --dump-phi-simp
        --dump-sco
        --dump-tre

        --dump-reg-alloc
        --dump-fpo
        --dump-cmp-elim
        --dump-reg-cprop
        --dump-peep-opt

        [fileName].c
```

*Note :- for iterative optimizations, dump flags only print the IR after all the iterations of that pass have completed and if an optimization runs in multiple parts fo the compilation phase, it gets printed that many times in the same order...*

### TAC and MIR track every single change made to them by any pass after construction and it can be printed using :

```
csalt --histor-dump-[compilation phase or optimization pass] [fileName].c
```

### TAC and MIR also hold specific metadata (dominator sets, liveness info, etc.) and it can be printed using :

```
csalt --meta-dump-[compilation phase or optimization pass] [fileName].c
```

### both of these dump modifiers can be applied together as well :

```
csalt --history-meta-dump-[compilation phase or optimization pass] [fileName].c
```

### finally, since the compiler does not support any way of printing outputs :

you need to extract the exit code of your program before the OS truncates it, thats the only way of printing something to the screen and the compiler does it for you whenever you compile any code, if you dont want that, you can disable output printing

```
csalt --disable-output [fileName].c
```
