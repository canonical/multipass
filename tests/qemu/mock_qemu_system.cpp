/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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
 */

#include <QJsonDocument>
#include <QJsonObject>

#include <iostream>

int main(int argc, char* argv[])
{
    std::string input;

    if (argc > 2 && strcmp(argv[2], "-dump-vmstate") == 0)
        return 0;

    while (true)
    {
        char c;
        std::cin.get(c);
        input += c;

        QJsonParseError parse_error;
        auto json = QJsonDocument::fromJson(QByteArray::fromStdString(input), &parse_error);

        if (parse_error.error != QJsonParseError::NoError)
        {
            continue;
        }
        else
        {
            auto json_object = json.object();

            auto execute = json_object["execute"];

            if (execute == "system_powerdown")
            {
                break;
            }
            else if (execute == "human-monitor-command")
            {
                auto args = json_object["arguments"].toObject();
                auto command_line = args["command-line"];
                if (command_line == "savevm suspend")
                {
                    std::cout << "{\"timestamp\": {\"seconds\": 1541188919, \"microseconds\": 838498}, \"event\": "
                                 "\"RESUME\"}";
                }
            }
            input.clear();
        }
    }
    return 0;
}
