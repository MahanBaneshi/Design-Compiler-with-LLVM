// Generates a DOT control-flow graph (CFG) for an LLVM function and writes it to a file.
#pragma once
#include <llvm/IR/Function.h>
#include <llvm/IR/BasicBlock.h>
#include <fstream>

inline void writeCFG(llvm::Function* F, const std::string& path) {
    std::ofstream out(path);
    out << "digraph CFG {\n";

    for (auto& BB : *F) {
        out << "  \"" << BB.getName().str() << "\";\n";
        for (auto* Succ : llvm::successors(&BB)) {
            out << "  \"" << BB.getName().str() << "\" -> \""
                << Succ->getName().str() << "\";\n";
        }
    }

    out << "}\n";
}
