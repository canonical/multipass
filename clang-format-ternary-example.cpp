// clang-format ternary operator formatting example

#include <string>

struct MockProcess
{
    std::string read_std_output();
    std::string read_std_error();
};

// Case 1: ternary in an assignment — the whole expression exceeds the column limit,
// so a line break is needed somewhere. Where and how differs between the two settings.
void example_assignment(bool success, MockProcess* proc)
{
    // Fits on one line — both settings leave it alone:
    auto& short_name = success ? proc->read_std_output() : proc->read_std_error();

    // Does not fit on one line — settings diverge:
    std::string long_variable_name_here =
        success ? proc->read_std_output() : proc->read_std_error();
}

// Case 2: chained ternary in an assignment — PenaltyBreakAssignment keeps the first
// condition on the assignment line; BreakBeforeTernaryOperators and AlignOperands
// then control how the remaining chain is indented.
void example_chained(int value)
{
    // Fits on one line — both settings leave it alone:
    int result = value < 0 ? -1 : value == 0 ? 0 : 1;

    // Does not fit on one line — settings diverge:
    int long_result_variable = value < 0        ? -100
                               : value == 0     ? 0
                               : value < 100    ? 100
                               : value < 100000 ? 100000
                                                : 9999999;
}
