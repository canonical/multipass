#!/bin/bash
# shellcheck disable=all

VENV=".sphinx/venv/bin/activate"

files=0
words=0
readabilityWords=0
readabilitySentences=0
readabilitySyllables=0
readabilityAverage=0
readable=true

# measure number of files (.rst and .md), excluding those in .sphinx dir
files=$(find . -type d -path './.sphinx' -prune -o -type f \( -name '*.md' -o -name '*.rst' \) -print | wc -l)

# calculate metrics only if source files are present
if [ "$files" -eq 0 ]; then
    echo "There are no source files to calculate metrics"
else
    # measure raw total number of words, excluding those in .sphinx dir
    words=$(find . -type d -path './.sphinx' -prune -o \( -name '*.md' -o -name '*.rst' \) -exec cat {} + | wc -w)

    # calculate readability for markdown source files
    echo "Activating virtual environment to run vale..."
    source "${VENV}"

    for file in *.md *.rst; do
        if [ -f "$file" ]; then
            readabilityWords=$(vale ls-metrics "$file" | grep '"words"' | sed 's/[^0-9]*//g')
            readabilitySentences=$(vale ls-metrics "$file" | grep '"sentences"' | sed 's/[^0-9]*//g')
            readabilitySyllables=$(vale ls-metrics "$file" | grep '"syllables"' | sed 's/[^0-9]*//g')
        fi
    done

    echo "Deactivating virtual environment..."
    deactivate

    # calculate mean number of words
    if [ "$files" -ge 1 ]; then
        meanval=$((readabilityWords / files))
    else
        meanval=$readabilityWords
    fi

    readabilityAverage=$(echo "scale=2; 0.39 * ($readabilityWords / $readabilitySentences) + (11.8 * ($readabilitySyllables / $readabilityWords)) - 15.59" | bc)

    # cast average to int for comparison
    readabilityAverageInt=$(echo "$readabilityAverage / 1" | bc)

    # value below 8 is considered readable
    if [ "$readabilityAverageInt" -lt 8 ]; then
        readable=true
    else
        readable=false
    fi

    # summarise latest metrics
    echo "Summarising metrics for source files (.md, .rst)..."
    echo -e "\ttotal files: $files"
    echo -e "\ttotal words (raw): $words"
    echo -e "\ttotal words (prose): $readabilityWords"
    echo -e "\taverage word count: $meanval"
    echo -e "\treadability: $readabilityAverage"
    echo -e "\treadable: $readable"
fi
