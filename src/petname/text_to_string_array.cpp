/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
void usage(char* argv[])
{
    std::cout << "Usage:\n  ";
    std::cout << argv[0] << " <adjectives> <adverbs> <names> <output>\n";
}

std::vector<std::string> words_in(const std::string& filename)
{
    std::ifstream input_stream(filename);
    std::string word;
    std::vector<std::string> words;
    while (std::getline(input_stream, word))
    {
        words.push_back(word);
    }
    return words;
}

class Words
{
public:
    Words(const std::string& filename, std::string var_name) : var_name{std::move(var_name)}, words{words_in(filename)}
    {
    }

    void print_to(std::ostream& out)
    {
        out << "const char* " << var_name << "[] =\n{\n";
        for (auto const& w: words)
        {
            out << "    \"" << w << "\",\n";
        }
        out << "};\n\n";
    }
private:
    std::string var_name;
    std::vector<std::string> words;
};
}

int main(int argc, char* argv[])
try
{
    if (argc != 5)
    {
        usage(argv);
        return EXIT_FAILURE;
    }

    Words adjectives{argv[1], "adjectives"};
    Words adverbs{argv[2], "adverbs"};
    Words names{argv[3], "names"};

    std::ofstream out(argv[4]);

    out << "//Auto Generated, any edits will be lost\n\n";
    out << "namespace multipass\n{\n";
    out << "namespace petname\n{\n";

    adjectives.print_to(out);
    adverbs.print_to(out);
    names.print_to(out);

    out << "}\n}";

    return EXIT_SUCCESS;
}
catch(const std::exception& e)
{
    std::cerr << "Error: " << e.what() << "\n";
}
