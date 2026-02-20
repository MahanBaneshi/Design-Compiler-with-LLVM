# Design Compiler with LLVM

A custom compiler implementation built using LLVM infrastructure. This
project demonstrates the design and implementation of a compiler
pipeline including lexical analysis, parsing, semantic analysis,
intermediate representation (IR) generation, and native code generation
using LLVM.

------------------------------------------------------------------------

# Overview

This project implements a full compiler for a custom-designed
programming language. The compiler translates high-level source code
into LLVM Intermediate Representation (IR), which can then be optimized
and compiled into machine code using LLVM tools.

The purpose of this project is to:

-   Understand compiler construction principles
-   Implement a full compilation pipeline
-   Integrate LLVM for backend code generation
-   Produce executable machine code from custom language programs

------------------------------------------------------------------------

# Compiler Architecture

The compiler follows a traditional multi-stage architecture:

Source Code\
→ Lexical Analysis\
→ Parsing\
→ AST Construction\
→ Semantic Analysis\
→ LLVM IR Generation\
→ Optimization\
→ Machine Code Generation

Each stage is modular and separated logically in the codebase.

------------------------------------------------------------------------

# Compilation Pipeline

## 1. Lexical Analysis

The lexer converts raw source code into tokens such as:

-   Keywords
-   Identifiers
-   Literals
-   Operators
-   Delimiters

## 2. Parsing

The parser processes tokens and builds an Abstract Syntax Tree (AST)
according to the language grammar.

Supported constructs typically include:

-   Variable declarations
-   Arithmetic expressions
-   Conditional statements
-   Loops
-   Function definitions
-   Function calls
-   Return statements

## 3. Semantic Analysis

This stage ensures:

-   Type correctness
-   Scope validation
-   Symbol table consistency
-   Function signature matching
-   Proper variable usage

## 4. LLVM IR Generation

The AST is translated into LLVM IR using LLVM APIs such as:

-   LLVMContext
-   Module
-   IRBuilder
-   Function
-   BasicBlock

The generated IR can be:

-   Printed as .ll file
-   JIT executed
-   Compiled into object code

------------------------------------------------------------------------

# Language Features

The custom language includes:

### Data Types

-   int
-   float
-   bool
-   void

### Operators

-   Arithmetic: +, -, \*, /
-   Comparison: ==, !=, \<, \>, \<=, \>=
-   Logical: &&, \|\|

### Control Flow

-   if / else
-   while / for

### Functions

-   User-defined functions
-   Parameters
-   Return values

------------------------------------------------------------------------

# Project Structure

Example structure:

    src/
      lexer/
      parser/
      ast/
      semantic/
      codegen/
    main.cpp
    CMakeLists.txt
    README.md

Module descriptions:

-   lexer/ → Token generation
-   parser/ → Syntax analysis
-   ast/ → AST node definitions
-   semantic/ → Symbol tables and type checking
-   codegen/ → LLVM IR generation
-   main.cpp → Compiler entry point

------------------------------------------------------------------------

# Build Instructions

## Requirements

-   LLVM (version 14+ recommended)
-   CMake
-   C++17 compatible compiler (clang++ or g++)

## Install LLVM (Linux)

``` bash
sudo apt install llvm clang
```

## Build Steps

``` bash
mkdir build
cd build
cmake ..
make
```

------------------------------------------------------------------------

# Running the Compiler

Compile a source file:

``` bash
./compiler input.my
```

Generate LLVM IR:

``` bash
./compiler input.my -emit-llvm
```

Execute using LLVM interpreter:

``` bash
lli output.ll
```

Compile to native executable:

``` bash
llc output.ll -filetype=obj
clang output.o -o program
./program
```

------------------------------------------------------------------------

# Example

Example source program:

    int main() {
        int a = 5;
        int b = 10;
        return a + b;
    }

The compiler generates LLVM IR including:

-   Function definition
-   Alloca instructions
-   Load/store instructions
-   Add instruction
-   Return instruction

------------------------------------------------------------------------

# Optimization

LLVM allows multiple optimization passes such as:

-   Constant propagation
-   Dead code elimination
-   Loop optimizations
-   Instruction combining

Optimization example:

``` bash
opt -O2 output.ll -o optimized.ll
```

------------------------------------------------------------------------

# Error Handling

The compiler includes:

-   Syntax error reporting with line numbers
-   Type mismatch detection
-   Undeclared variable errors
-   Function signature mismatch errors

These diagnostics help users debug source programs effectively.

------------------------------------------------------------------------

# Future Improvements

-   Add arrays and structs
-   Add object-oriented features
-   Improve error recovery
-   Add unit testing framework
-   Implement REPL mode
-   Improve optimization pipeline
-   Add JIT execution mode

------------------------------------------------------------------------

# Author

Mahan Baneshi _ Compiler Design Project implemented using LLVM infrastructure.
