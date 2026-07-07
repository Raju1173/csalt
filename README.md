# CSalt
An optimization focused compiler for a minimal subset of C targeting x86-64 assembly

# Performance Benchmarks

- **Benchmark command** : `hyperfine -i -N ./[fileName]`
- **CPU**: Intel i5-13420H (13th Gen)
- **Clang Version**: 18.1.3
- **GCC Version**: 13.3.0

| Compiler |  |
|---|---|
| CSalt |  |
| Clang -O1 |  |
| GCC -O1 |  |

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

- Unary operators *(use '(0 - x)' for negation)*
- Structs
- Unions
- Arrays
- Pointers
- Any other keyword except int, if, while or return
- Preprocessor statements
- Variable shadowing *(important)*
- Other fancy C features like "int x, y;", etc.

# Architecture

`Source Code -> Tokens -> AST -> CFG -> SSA -> TAC -> Middle End optimizations -> Machine IR -> Backend Optimizations -> Assembly`

*Note : I'm storing AST nodes inside the CFG because it felt much simpler than converting from AST to TAC directly...*

# Limitations

### **No error detection**
csalt treats incorrect code as undefined behaviour. *(definitely not a sophisticated way of saying "I was too lazy to do semantic analysis")*
### **No support for external libraries**
unfortunately, some external libraries require the stack frame to be aligned with 16 bytes for SIMD instructions but csalt does not perform any alignment
### **Exit codes as the only method of getting an output**
since csalt does not support external libraries, there is no way of printing to the terminal other than returning an integer and catching it using "echo $?" or using a parent process to fetch the value in rdi during termination before the OS truncates it to 8 bits...

# Usage

### CSalt can only compile a single file with no dependencies :

`csalt fileName`

### You can inspect the output of every single compilation step individually using flags :

`csalt dump-tok dump-ast dump-cfg dump-tac dump-asm fileName`\
or\
`csalt dump-all fileName`
