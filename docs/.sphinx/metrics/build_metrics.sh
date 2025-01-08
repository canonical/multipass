#!/bin/bash
# shellcheck disable=all

links=0
images=0

# count number of links
links=$(find . -type d -path './.sphinx' -prune -o -name '*.html' -exec cat {} + | grep -o "<a " | wc -l)
# count number of images
images=$(find . -type d -path './.sphinx' -prune -o -name '*.html' -exec cat {} + | grep -o "<img " | wc -l)

# summarise latest metrics
echo "Summarising metrics for build files (.html)..."
echo -e "\tlinks: $links"
echo -e "\timages: $images"
