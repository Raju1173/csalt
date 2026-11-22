# Testing

`csalt-tester` is a small automated testing system for this compiler.

It recursively looks for C files in a specified directory, compiles each one under multiple optimization configurations, and compares the program's output against an expected value.

## Usage

```
csalt-tester --[permanent flag 1] --[permanent flag 2] ... --[permanent flag N] [test directory]
```

Permanent flags are passed to every compiler invocation. The final argument is the directory containing the test files.

For example:

```
csalt-tester --some-flag --another-flag ./tests
```

## Test File Format

Every test file must have an expected output declaration in its first non empty line.

```
// EXPECTED : 42

int main()
{
    return 42;
}
```

The tester recursively searches the supplied directory for `.c` files and ignores other file types.

## Testing Process

### Optimization Configurations

Each test is compiled under multiple optimization configurations.

Two configurations are always included:

- Default compiler configuration.
- All optimizations disabled (`--disable-all`).

Additionally, for every optimization target in `targetPhaseMap` (csalt.h) :

- Disable only that optimization (`--disable-[target]`).
- Disable all optimizations and enable only that optimization (`--disable-all --enable-[target]`).

This serves two purposes:

**Isolation:** Enabling one optimization at a time makes it easier to identify passes that introduce incorrect output.

**Regression testing:** Disabling individual optimizations helps identify cases where the compiler relies on a particular pass or where an optimization interacts incorrectly with the rest of the pipeline.

Permanent flags supplied through the command line are appended to every configuration.

Full combinatorial testing with every possible flag combination would have been extremely difficult to optimize to run in a feasible time when I wanted quick testing for faster iterations...

### Compilation and Execution

Each configuration invokes the compiler through a shell command:

```
timeout 5s csalt [flags] [file]
```

The tester captures both standard output and standard error.

Compilation is considered unsuccessful if the compiler exits with a non-zero status. Compilation is also considered timed out if the external timeout command returns status `124`.

The tester additionally inspects the compiler's output for execution related messages:

- `COMPILED CODE'S EXECUTION TIMED OUT`
- `COMPILED CODE'S EXECUTION FAILED`
- `OUTPUT OF COMPILED CODE`

The last marker is followed by the program's output, which the tester extracts and parses as an integer.

The resulting value is compared against the expected output declared in the test file.

### Parallel Testing

Individual test files are processed concurrently using multiple threads.

All optimization configurations for a particular file are tested sequentially by its assigned worker.

This improves the overall testing speed by ~10 times compared to naive sequential testing and I believe there's still room for further improvements but for now, its fast enough for my workflow.

### Results

Each test configuration can fail because of:

- Compilation timeout.
- Compilation failure.
- Program execution timeout.
- Program execution failure.
- Output mismatch.

Failed configurations print the filename, reason for failure, and compiler flags used.

A test file is marked `[PASS]` only when all its configurations pass. Otherwise, individual failures are reported separately.

## Final Thoughts

The current implementation is intentionally fairly small. It covers the testing workflow I needed while developing the compiler, particularly checking whether optimization passes preserve program output across different configurations.
