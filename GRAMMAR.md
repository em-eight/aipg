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

## Labels
Labels are currently not supported. For now, the generated parsers only look at the corresponding binary code 
(usually 0 if there is a reloc and the label value (relative or absolute depending on the instruction) otherwise).
