#include <ostream>
#include <print>
#include <string>
#include <fstream>
#include <vector>
#include "lexer.h"

int main(int argc, char** argv)
{
    if(argc != 2)
    {
	std::print("Error : Exactly one file name expected\n");
	
	return 1;
    }

    std::ifstream file(argv[1]);

    if (!file.is_open())
    {
	std::print("Error : Could not open file\n");
	return 1;
    }

    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::vector<Token> TokenStream = tokenize(source);

    for(Token t : TokenStream)
    {
	if(t.type == TokenType::IDENTIFIER || t.type == TokenType::NUMBER)
	    std::print("{}({})\n", TokenNames[std::to_underlying(t.type)], t.lexeme);
	else
	    std::print("{}\n", TokenNames[std::to_underlying(t.type)]);
    }

    return 0;
}
