# Assembly Idiom Parser Generator
![build badge](https://github.com/em-eight/aipg/actions/workflows/test.yml/badge.svg?branch=main)

Write human readable assembly idioms and generate fast PowerPC parsers
- The idiom grammar is human-readable and PowerPC assembly-like, see [GRAMMAR.md](https://github.com/em-eight/aipg/blob/main/GRAMMAR.md)
- The generated parser is (afaik) nearly optimal, only checking whatever is needed and at the binary level, so no disassembly is performed.
- After parsing, if some code matches, a `Context` struct is returned that can be used to retrieve the values of captured variables and the offset of each instruction that matched.

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
```
generates a parser that matches unsigned integer division.

You can invoke aipg to create a C++ parser for it
```
./aipg Udiv.idiom
```
which will generate `Udiv.hpp` for you. You can now include that file and use it to parse your binary, for example:
```cpp
#include <iostream>
#include "opcode/ppc.h"
#include "aipg/aipg.hpp"
#include "Udiv.hpp"

int main(int argc, char** argv) {
/*
original ASM:
lis     r3, 0x8889
lwz     r8, 0x14(r28)
addi    r0, r3, -0x7777
li      r3, 0
mulhw   r0, r0, r7
li      r3, 1
add     r0, r0, r7
srawi   r0, r0, 5
srwi    r5, r0, 0x1f
add     r6, r0, r5
*/
  uint32_t ins[] = {0x3c608889, 0x811c0014, 0x38038889, 0x38800000, 0x7c003896, 0x38600001, 0x7c003a14, 0x7c002e70, 0x54050ffe, 0x7cc02a14};
  aipg::Context parseCtx;
  bool match = aipg::matchUdiv(std::begin(ins), std::end(ins), PPC_OPCODE_PPC, parseCtx);
  std::cout << (match ? "Found match" : "No match found") << std::endl;
  for (uint32_t idx : parseCtx.matchInsIdxs)
    std::cout << "Matcing ins idx: " << idx << std::endl;
  for (auto& it : parseCtx.gprs) {
    std::cout << "GPR var " << it.first << " value is r" << it.second << std::endl;
  }
  for (auto& it : parseCtx.imms) {
    std::cout << "Immediate var " << it.first << " value is " << it.second << std::endl;
  }
}
```
which produces the following output:
```
Found match
Matcing ins idx: 0
Matcing ins idx: 2
Matcing ins idx: 4
Matcing ins idx: 7
GPR var 2 value is r0
GPR var 4 value is r0
GPR var 1 value is r7
GPR var 3 value is r0
GPR var 8 value is r0
GPR var 9 value is r3
Immediate var 3 value is 5
Immediate var 2 value is -30583
Immediate var 1 value is -30583
```

## Dependencies
Both the generator and the runtime parser depend on [ppcdisasm-cpp](https://github.com/em-eight/ppcdisasm-cpp).
The generator requires a compiler with c++17 support and the runtime parser c++20 support

## Building
- `mkdir build && cd build`
- `cmake ..`
- `make`

## Usage
### Command line
`./aipg aipg [--out output_parser_location file1.idiom file2.idiom ..`

### In build system
When using this project's parsers in your own project, usually you will want to perform the parser generation at build time, before your targets that use them are built. You can find an example of doing this with CMake in this project's [CMakeLists.txt](https://github.com/em-eight/aipg/blob/main/CMakeLists.txt)

## Limitations
- Currently, rotate-shift instruction idioms can only use the 4 argument forms.
- No SPR operands in idioms. You can get around this using extended mnemonics.