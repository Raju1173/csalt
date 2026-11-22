# Parser

This is probably THE most ugly part of this compiler.

I originally wrote it as an experiment to understand recursive descent and Pratt parsing at a deeper level. Instead of implementing either conventionally, I tried combining ideas from both into a single iterative loop using an explicit stack.

I'm genuinely quite proud of this implementation. It took some experimenting to figure out how to represent parsing context using a stack and construct expression trees through successive stack operations without recursive parsing calls.

It isn't anywhere near clean, and I wouldn't recommend anyone to try reading the code but getting it to work was extremely satisfying, and I learned a lot about parsing in the process.

## How It Works

### The Parsing Stack

The parser maintains a stack of partially constructed AST nodes.

Rather than having a separate recursive function for each grammar rule, the parser examines the type of the node currently at the top of the stack to determine how the current token should be handled.

For example:

- `PROGRAM` handles top level declarations.
- `FUNCTION` handles function parameters and its body.
- `BLOCK` handles statements inside a block.
- `IF` and `WHILE` handle their conditions and bodies.
- `EXPR` handles expressions.
- `BINARY_OP` and `UNARY_OP` handle operators and their operands.

Each of these represents a parsing context. When the parser encounters something that requires a new context, it pushes another node onto the stack.

Once that context has been fully parsed, the node is removed from the stack and attached to its parent.

Two small lambdas handle most of this bookkeeping:

**`pushNode()`**

Creates a new AST node and pushes it onto the stack. Its initial children can also be provided.

**`popAndAttach()`**

Removes the node at the top of the stack and attaches it to the node immediately below it.

This is how completed constructs gradually become part of the AST.

The important distinction is that the stack contains both partially constructed nodes and nodes that are waiting for their children to be parsed.

### Statement Parsing

Statement parsing is handled by switching on the current stack node's type.

For example, when the parser is inside a `BLOCK`, it examines the current token to determine what kind of statement is beginning.

- `IF` and `WHILE` create their corresponding nodes.
- `RETURN` creates a return node.
- An identifier followed by `(` starts a function call.
- An identifier followed by `=` starts either a variable declaration or an assignment expression.
- An identifier followed by `;` is treated as a variable declaration without an initializer, provided the preceding token is `INT`.

Function declarations are recognized at the `PROGRAM` level when an identifier is followed by `(`.

A `FUNCTION` node then pushes a `PARAMETERS` node upon encountering `(`. Parameter identifiers are appended directly to it until `)` is encountered.

Similarly, `IF` and `WHILE` nodes push an `EXPR` node when their opening parenthesis is encountered and a `BLOCK` node when their opening brace is encountered.

The same idea is used for variable initializers and return expressions.

Rather than recursively parsing these constructs, the parser simply pushes the appropriate node and lets the main loop handle the new parsing context.

### Expression Parsing

This is where most of the interesting stack manipulation happens.

Expression parsing primarily involves four node types:

- `EXPR` : Represents an expression currently being parsed.
- `BINARY_OP` : Represents a binary operator and its operands.
- `UNARY_OP` : Represents a unary operator and its operand.
- `NUMBER` and `IDENTIFIER` : Represent leaf operands.

When an `EXPR` node encounters a number or identifier, it appends the corresponding leaf node to its children.

When it encounters a binary operator, it converts its accumulated children into a `BINARY_OP` node.

For example, while parsing:

`a + b`

The stack initially contains an `EXPR` node.

After reading `a`, the expression node contains:

```
EXPR
|   IDENTIFIER(a)
```

Upon encountering `+`, the parser takes the accumulated children of `EXPR` and uses them to initialize a new binary operator node:

```
BINARY_OP(+)
|   IDENTIFIER(a)
```

The parser can now continue reading the right operand.

When the second operand is encountered, it is attached to the binary operator. Once the expression is complete, the binary operator is attached to its parent.

This is also how nested expressions are constructed without recursive parsing calls.

### Operator Precedence

The `precedence()` function assigns a numerical precedence to each supported binary operator.

When another operator is encountered, the parser compares its precedence with that of the binary operator currently being constructed.

Depending on the comparison, it either completes the current binary operator before processing the new one or starts a nested binary operator using the pending right operand.

This is what allows the parser to construct the correct expression tree without explicitly writing separate parsing functions for every precedence level.

Consider:

```
a + b * c
```

When the parser encounters `b` followed by `*`, it notices that multiplication has higher precedence than addition.

Instead of making `b` the right operand of addition immediately, it creates a multiplication node containing `b` and continues parsing its right operand.

The resulting tree is:

```
BINARY_OP(+)
|   IDENTIFIER(a)
|   BINARY_OP(*)
|   |   IDENTIFIER(b)
|   |   IDENTIFIER(c)
```

For:

```
a * b + c
```

Multiplication is completed before addition is processed:

```
BINARY_OP(+)
|   BINARY_OP(*)
|   |   IDENTIFIER(a)
|   |   IDENTIFIER(b)
|   IDENTIFIER(c)
```

The parser uses `>=` when comparing precedences, so operators with equal precedence are reduced before another operator of the same precedence is incorporated. This gives left to right grouping for operators at the same precedence.

### Unary Operators

Unary operators are handled separately using `UNARY_OP` nodes.

When an operator is encountered in an expression without an accumulated left operand, the parser treats it as unary and pushes a `UNARY_OP` node.

For example:

```
-a
```

Produces:

```
UNARY_OP(-)
|   IDENTIFIER(a)
```

The parser also has special handling for numeric unary expressions. Instead of retaining a separate unary node, it directly prefixes the numeric token's lexeme with `-` and creates a negative number node.

Parenthesized expressions and function calls can also appear while parsing unary operands.

### Parentheses and Function Arguments

Parenthesized expressions are handled by pushing another `EXPR` node.

Once the closing parenthesis is encountered, the inner expression is attached to the waiting outer context.

The exact attachment depends on what is currently being parsed. For example, an inner expression can become an operand of a binary operator, an operand of a unary operator, or an argument of a function call.

Function calls use a similar mechanism.

When an identifier is followed by `(`, a `CALL` node is pushed. Each argument is parsed using an `EXPR` node. Commas terminate individual argument expressions, while the closing parenthesis completes the call.

The parser also handles nested function calls through the same mechanism.

### Expression Completion

Expressions can terminate at `)`, `,`, or `;`.

The parser pops the current expression and attaches its result to the waiting parent.

There is a small distinction here, when the parent is another expression or a binary operator, the parser attaches the expression's resulting child directly rather than retaining the intermediate `EXPR` wrapper.

For other parents, such as `CALL`, `VAR`, and `RETURN`, the expression node itself is attached.

This allows the same expression parsing logic to be reused in several different contexts without having to know whether the expression represents a return value, function argument, condition, or initializer.

Once the entire TokenStream has been processed, the root node at the top of the stack is assigned to `AST`.

And that's the end of the parsing phase.

## Problems and Limitations

Almost everything is a problem in this parser but some of the major ones are :

- **Readability :** The nested switch statements make the parser difficult to follow.
- **Extensibility :** Adding new grammar rules requires modifying several existing branches and can be considered equivalent to medieval torture.
- **Potential infinite loops/segmentation faults :** An unhandled unexpected token sequence can leave the parser stuck processing the same token forever.

## Final Thoughts

I know this isnt maintainable code, and I'm not going to pretend otherwise.

But this parser was an experiment in understanding how parsing works at a fundamental level, not an attempt to create a new parsing algorithm.

I wanted to see whether I could combine recursive descent style parsing contexts with Pratt style operator precedence handling, without relying on recursive parsing functions.

The result is this weird stack based parser.

And I was able to make it work.
