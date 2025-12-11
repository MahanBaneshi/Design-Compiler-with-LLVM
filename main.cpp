#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include "Lexer.h"
#include "Token.h"
#include "parser.h"
#include "ast_printer.h"

int main(int argc, char** argv) {

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

    // === مرحله ۱: Lexer ===
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenizeAll();

    // حتما توکن EOF آخر لیست باشد
    if (tokens.empty() || tokens.back().type != TokenType::END_OF_FILE) {
        tokens.emplace_back(TokenType::END_OF_FILE, "", 0, 0);
    }

    std::cout << "\n=== TOKENS ===\n";
    for (const Token& tok : tokens) {
        std::cout
            << "Line " << tok.line
            << ", Col " << tok.column
            << " | " << tokenTypeToString(tok.type)
            << " | \"" << tok.lexeme << "\"\n";
    }

    // === مرحله ۲: Parser ===
    Parser parser(tokens);
    ASTNodePtr ast;

    try {
        ast = parser.parseProgram();
    } catch (const ParseException&) {
        // نباید به اینجا برسیم چون کد داخل Parser خودش sync می‌کند،
        // ولی اگر رسیدیم یعنی خطای نحوی ثبت شده است.
    }

    if (parser.hasErrors()) {
        const auto& errs = parser.getErrors();
        std::cerr << "\n=== PARSE ERRORS (" << errs.size() << ") ===\n";
        for (std::size_t i = 0; i < errs.size(); ++i) {
            const auto& e = errs[i];
            std::cerr << "  [" << (i + 1) << "] "
                      << "Line " << e.line << ", Col " << e.column
                      << " : " << e.message << "\n";
        }
    } else {
        std::cout << "\n=== PARSE OK ===\n";
        printAST(ast);
    }

    return 0;
}
