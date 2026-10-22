# Expression Compiler to Bytecode

A C++17 expression compiler with tokenization, precedence parsing, an AST, constant folding, and a stack virtual machine. It supports `+`, `-`, `*`, `/`, exponentiation, parentheses, variables, and `sin`, `cos`, `sqrt`, `abs`, `min`, and `max` calls. The CLI prints bytecode and the resulting value.

```sh
g++ -std=c++17 -O2 -Wall -Wextra -pedantic main.cpp -o expr
./expr "2*3+x" x=4
python tests/test_cli.py
```

Invalid syntax, missing variables, division by zero, and unsupported functions return an error. Constant subtrees compile to a single push instruction.

