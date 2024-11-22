#include <cstdlib>
#include <iostream>
#include <string>

[[noreturn]] void crash()
{
#ifdef MULTIPASS_PLATFORM_WINDOWS
    // Prevent Windows from making a dialogue box and enable crash data reporting.
    // If data reporting is not enabled, QProcess will always return NormalExit.
    _set_abort_behavior(_CALL_REPORTFAULT, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    abort();
}

int main(int argc, char* argv[])
{
    if (argc == 1)
    {
        // deliberately will crash if argument 1 missing
        crash();
    }
    // Exit immediately if only 1 argument
    else if (argc == 2)
    {
        return atoi(argv[1]);
    }

    // If more than 1 argument, run until get EOF from input
    std::string s;
    std::getline(std::cin, s, '\0');

    // Crash on demand
    if (s == "crash")
    {
        crash();
    }

    // Print out what was supplied by stdin
    std::cout << s;
    std::cerr << s;

    return atoi(argv[1]);
}
