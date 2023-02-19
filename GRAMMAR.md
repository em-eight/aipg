# Idiom grammar
Idiom grammar is very similar to the usual PowerPC assembly, with additional support for the following value variables, depending on the type of expected operand

## Registers
### General purpose registers
- $GPR1, $GPR2, etc. The registers these variables refer to can be retrieved after parsing through the `Context` variable
- $GPR? wildcard that matches any GPR

### Floating point registers
- $FPR1, $FPR2, etc. The registers these variables refer to can be retrieved after parsing through the `Context` variable
- $FPR? wildcard that matches any GPR

When given a piece of code to match, every occurence of a GPR variable must correspond to the same register value in order for the idiom to match.

When defined register values are provided (e.g. `r1`), the parser checks that it references that operand (duh).

## Immediates
Similar to registers, immediates can either be provided as literals, in which case they are checked if they match, or as immediate variables 
($IMM1, $IMM2, etc) which follow the same rules as GPR variables.

- $IMM1, $IMM2, etc. The immediates these variables refer to can be retrieved after parsing through the `Context` variable
- $IMM? wildcard that matches any immediate value

## Labels (and relocations)
Again, similar to immediates. Labels can either be provided as a symbol name, in which case they are checked if they match, or as immediate variables 
($LAB1, $LAB2, etc) which follow the same rules as GPR variables. In order for labels to be supported though, a starting memory address and relocation information needs to be provided (see label tests at `test/parse_test.cpp` for examples).

- $LAB, $LAB2, etc. The immediates these variables refer to can be retrieved after parsing through the `Context` variable
- $LAB? wildcard that matches any immediate value
- labels (either defined or variable) can be postfixed with `@ha`, `@h`, `@l`, or `@sda21` in order to apply an additional check if the label reference has the correct relocation.

## Special lines
Use `...` to match any number of any instruction (as few as possible)
### Instruction constraints
You can add constraints on the instructions consumed by `...`, by adding one or more of the following expressions after `...`:
- `{xxx}` Specifies that the instructions may only read registers that are mentioned inside the brackets
- `[xxx]` Specifies that the instructions may only write registers that are mentioned inside the square brackets
- `^{xxx}` Specifies that the instructions must NOT read registers that are mentioned inside the brackets
- `^{xxx}` Specifies that the instructions must NOT write registers that are mentioned inside the brackets

where `xxx` is a comma separated list of float or general purpose registers, either variables or defined.

## Comments
The idiom grammar supports C-style inline and multiline comments
- `// This a comment`
- `/* This is another comment */`