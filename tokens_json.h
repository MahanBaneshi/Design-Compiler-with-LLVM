// tokens_json.h
#pragma once
#include "Token.h"
#include <vector>
#include <string>
#include <sstream>

inline std::string tokensToJson(const std::vector<Token>& tokens) {
    std::ostringstream out;
    out << "[\n";

    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto& t = tokens[i];
        out << "  { "
            << "\"type\": \"" << tokenTypeToString(t.type) << "\", "
            << "\"lexeme\": \"" << t.lexeme << "\", "
            << "\"line\": " << t.line << ", "
            << "\"column\": " << t.column
            << " }";
        if (i + 1 < tokens.size()) out << ",";
        out << "\n";
    }

    out << "]";
    return out.str();
}
