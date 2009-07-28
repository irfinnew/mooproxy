#! /bin/sh

# The cats make sed go into block-buffered mode, so output is batched up.
# Use textwidth=62 when editing the config file.

cat | sed 's/"/\\"/g;s/^#\? \?/"/;$!s/$/\\n"/;$s/$/" },/' | cat
