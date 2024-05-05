#include <iostream>
#include <string>
#include <vector>

#include "udp.hh"

std::vector<char> buffer(1024, 0);

auto main(void) -> int
{
    jj::UDP server{"0.0.0.0", "5000", jj::UDP::Side::SERVER};
    std::cout << "Starting listener on port: 5000" << std::endl;

    while (true)
    {
        server >> buffer;
        std::cout << buffer.data() << std::endl;
    }

    return EXIT_SUCCESS;
}
