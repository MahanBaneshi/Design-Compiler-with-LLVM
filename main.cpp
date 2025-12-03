#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include "Lexer.h"
#include "Token.h"

int main(int argc, char** argv) {
    // اگر اسم فایل ندادند، راهنمای ساده چاپ کن
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <source-file>\n";
        return 1;
    }

    const char* filename = argv[1];
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Error: cannot open file: " << filename << "\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string source = buffer.str();

    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenizeAll();

    std::cout << "Tokens for file: " << filename << "\n\n";

    for (const Token& tok : tokens) {
        std::cout 
            << "Line " << tok.line 
            << ", Col " << tok.column 
            << " | " << tokenTypeToString(tok.type)
            << " | \"" << tok.lexeme << "\"";

        if (tok.type == TokenType::INVALID) {
            std::cout << "   <-- INVALID TOKEN";
        }

        if (tok.type == TokenType::END_OF_FILE) {
            std::cout << "   (EOF)";
        }

        std::cout << "\n";
    }

    return 0;
}
