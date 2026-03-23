

#include "petname_generator.h"
#include <namegen/src/lib.rs.h>

namespace mp = multipass;

mp::PetnameGenerator::PetnameGenerator(mp::petname::NumWords num_words, char separator)
    : num_words{num_words}, separator{separator}
{
}

std::string mp::PetnameGenerator::make_name()
{
    return std::string(rxx::petname::make_petname(num_words, separator));
}
