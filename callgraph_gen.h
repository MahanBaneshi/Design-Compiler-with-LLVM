// Generates a DOT call graph for all functions in an LLVM module and writes it to a file.
#pragma once
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <fstream>

inline void writeCallGraph(llvm::Module* M, const std::string& path) {
    std::ofstream out(path);
    out << "digraph CallGraph {\n";

    for (auto& F : *M) {
        if (F.isDeclaration()) continue;

        out << "  \"" << F.getName().str() << "\";\n";

        for (auto& BB : F) {
            for (auto& I : BB) {
                if (auto* call = llvm::dyn_cast<llvm::CallBase>(&I)) {
                    if (auto* callee = call->getCalledFunction()) {
                        out << "  \"" << F.getName().str()
                            << "\" -> \"" << callee->getName().str()
                            << "\";\n";
                    }
                }
            }
        }
    }

    out << "}\n";
}
