// Provides simple utilities for timing and counting AST nodes.
#pragma once
#include <chrono>
#include "parser.h"

inline uint64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

inline uint64_t countAST(const ASTNodePtr& n) {
    if (!n) return 0;
    uint64_t c = 1;
    for (auto& ch : n->children) c += countAST(ch);
    return c;
}
