#include <cstdlib>
#include <iostream>

int main(int /*argc*/, char* argv[])
{
    // deliberately will crash if argument 1 missing
    return atoi(argv[1]);
}
