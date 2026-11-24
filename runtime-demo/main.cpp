#include <exception>
#include <iostream>

#include "RuntimeDemo.hpp"

int main(int argc, char** argv)
{
    try
    {
        RuntimeDemo demo;
        return demo.run(argc, argv);
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Comparison failed: " << exception.what() << '\n';
        return 1;
    }
}
