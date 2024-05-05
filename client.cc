#include <iostream>
#include <string>

#include "udp.hh"

auto main(int argc, char** argv) -> int
{
    if (argc != 2)
    {
        std::cout << "Pass in the server IP" << std::endl;
    }

    jj::UDP client(argv[1], "5000", jj::UDP::CLIENT);
    std::string msg;

    while (true)
    {
        std::getline(std::cin, msg);
        client << msg;
    }
    return EXIT_SUCCESS;
}
