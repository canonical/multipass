#!/usr/bin/env python3
#
# Copyright (C) Canonical, Ltd.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 3.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
#

"""Random goofy name generator for the test VMs with a touch of middle-earth."""

import random

prefixes = [
    "gondorian",
    "rohirrim",
    "elven",
    "dwarven",
    "orcish",
    "hobbitish",
    "entish",
    "numenorean",
    "shire",
    "rivendell",
    "mordorian",
    "fangorn",
    "mirkwood",
    "erebor",
    "lothlorien",
    "isengard",
    "mithril",
    "palantir",
    "uruk",
    "balrog",
    "shadowfax",
    "anduril",
    "eagle",
    "wizardly",
]

middles = [
    "potato",
    "pipeweed",
    "lembas",
    "ring",
    "nazgul",
    "gollum",
    "gimli",
    "frodo",
    "samwise",
    "aragorn",
    "legolas",
    "boromir",
    "saruman",
    "gandalf",
    "bilbo",
    "smeagol",
    "mountdoom",
    "shireling",
    "oliphaunt",
    "balrog",
    "eomer",
    "theoden",
    "galadriel",
    "elrond",
    "orc",
    "halfling",
    "took",
    "baggins",
    "shirepony",
    "sting",
    "sauron",
    "denethor",
]

suffixes = [
    "brigade",
    "council",
    "fellowship",
    "posse",
    "taskforce",
    "alliance",
    "order",
    "company",
    "legion",
    "patrol",
    "guard",
    "wardens",
    "watch",
    "band",
    "guild",
    "army",
    "horde",
    "circle",
    "division",
    "host",
    "court",
    "troop",
    "league",
    "pack",
]


def random_vm_name():
    return f"{random.choice(prefixes)}-{random.choice(middles)}-{random.choice(suffixes)}-{random.randint(0, 9999):04d}"
