# Assembly Idiom Parser Generator
Write human readable assembly idioms and generate fast PowerPC parsers
- The idiom grammar is human-readable and PowerPC assembly-like, see [GRAMMAR.md](https://github.com/em-eight/aipg/blob/main/GRAMMAR.md)
- The generated parser is (afaik) nearly optimal, only checking whatever is needed and at the binary level, so no disassembly is performed.
- After parsing, if some code matches, a `Context` struct is returned that can be used to retrieve the values of captured variables.

## Example
The idiom
```
lis      $GPR9,$IMM1
...
addi     $GPR8,$GPR9,$IMM2
...
mulhw    $GPR3,$GPR8,$GPR1
...
srawi    $GPR4,$GPR2,$IMM3
...
add      $GPR5,$GPR3,$GPR4
```
generates a parser that matches unsigned integer division. After matching, the `Context` variable will contain the register being divided at `$GPR1`,
the magic value at `$IMM1`,`$IMM2`, the shift amount at `$IMM3` and the result register at `$GPR5`.

## Building
- `mkdir build && cd build`
- `cmake ..`
- `make`

## Usage
`./aipg aipg [--src_out src_out] [--inc_out inc_out] file1.idiom file2.idiom ..`
