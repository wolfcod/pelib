// tests.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <iostream>
#include <pelib/pelib.hpp>
#include <utility>

class X
{
    int* n;

    public:
        X() { 
            n = new int[100]; 
            for (int i = 0; i < 100; i++)
                n[i] = i;
        };
        
        X(const X& other)
        {
            std::cout << "copy values by arg" << std::endl;

            n = new int[100];
            for (int i = 0; i < 100; i++)
                n[i] = other.n[i];
        }

        X(const X&& other)
        {
            std::cout << "rvalue ref constructor..." << std::endl;
            n = std::move(other.n);
        }

        ~X()
        {
            if (n != nullptr) {
                std::cout << "Destroying data at " << std::hex << (n) << std::endl;
                delete n;
            }
        }

        void print()
        {
            std::cout << "Pointer to n " << std::hex << (n) << std::endl;
        }

        static X create()
        {
            X f;

            f.print();

            return f;
        }
};

//int f() { return 0; };


int main(int argc, char *argv[])
{
    /*pelib::peloader loader;

    if (argc != 1)
        loader.load(argv[1]);
    else
    {
        // my pc..
        loader.load("c:\\tools\\putty.exe");
    }
     

    loader.sort();

    loader.removeSection(std::string(".text"));
    */

    X f = X::create();

    f.print();

    X n = f;

    return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
