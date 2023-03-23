#include <Base64.hpp>
#include <iostream>

int main(int argc, char** argv)
{
    std::string input = "HelloWorld";
    std::cout << input << ": " << Base64::encode(input) << std::endl;
    return 0;
}