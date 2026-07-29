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

# Optimization Pipeline

# Language Features

CSalt only supports 6 features of C :

- int
- if
- while
- local variables
- functions
- return

Keeping the fronted super small *(like a grain of sea salt)* makes implementing complex optimizations extremely simple and some even become almost trivial

# Unsupported Features

- else blocks *(important, use consequent if blocks)*
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

### You can inspect the output of every single compilation step individually using flags :

`csalt dump-tok dump-ast dump-cfg dump-tac dump-asm fileName`\
or\
`csalt dump-all fileName`
