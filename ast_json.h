// Serializes the AST into a simple JSON-like string representation.
#pragma once
#include "parser.h"
#include <string>
#include <sstream>

inline void astToJsonRec(const ASTNodePtr& n, std::ostringstream& out, int indent) {
    if (!n) return;

    auto indentStr = std::string(indent, ' ');

    out << indentStr << "{\n";
    out << indentStr << "  \"type\": \"" << (int)n->type << "\",\n";
    out << indentStr << "  \"lexeme\": \"" << n->token.lexeme << "\",\n";
    out << indentStr << "  \"children\": [\n";

    for (size_t i = 0; i < n->children.size(); ++i) {
        astToJsonRec(n->children[i], out, indent + 4);
        if (i + 1 < n->children.size()) out << ",";
        out << "\n";
    }

    out << indentStr << "  ]\n";
    out << indentStr << "}";
}

inline std::string astToJson(const ASTNodePtr& root) {
    std::ostringstream out;
    astToJsonRec(root, out, 0);
    return out.str();
}
