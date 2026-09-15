#!/usr/bin/env bash
# Run in your normal terminal, not with sudo. Optionally supply an aliases file.
set -euo pipefail

aliases_file="${1:-${HOME}/.bash_aliases}"
old_alias="alias codex='codex -a never -s workspace-write'"
new_alias="alias codex='codex -a on-request -s workspace-write'"

if (( $# > 1 )); then
    printf 'Usage: bash %s [aliases-file]\n' "$0" >&2
    exit 1
fi
if [[ ! -f "$aliases_file" || -L "$aliases_file" ]]; then
    printf 'Expected a regular, non-symlink aliases file: %s\n' "$aliases_file" >&2
    exit 1
fi

alias_count=$(grep -Ec '^[[:space:]]*alias[[:space:]]+codex=' "$aliases_file" || true)
if [[ "$alias_count" != 1 ]]; then
    printf 'Expected exactly one Codex alias; leaving the file unchanged.\n' >&2
    exit 1
fi
if grep -Fxq "$new_alias" "$aliases_file"; then
    printf 'Codex alias is already configured for approval prompts.\n'
elif grep -Fxq "$old_alias" "$aliases_file"; then
    backup=$(mktemp "${aliases_file}.backup.XXXXXXXX")
    cp -p -- "$aliases_file" "$backup"
    staged=$(mktemp "${aliases_file}.updated.XXXXXXXX")
    trap 'rm -f -- "$staged"' EXIT
    awk -v old="$old_alias" -v new="$new_alias" \
        '{ if ($0 == old) print new; else print }' "$aliases_file" > "$staged"
    bash -n "$staged"
    chmod --reference="$aliases_file" "$staged"
    mv -- "$staged" "$aliases_file"
    trap - EXIT
    printf 'Updated Codex alias. Backup: %s\n' "$backup"
else
    printf 'Codex alias differs from the expected original; leaving it unchanged.\n' >&2
    exit 1
fi

printf '\nRun in your current terminal:\n  source %q\n  codex\n' "$aliases_file"
printf '\nStart a new Codex session to use approval prompts. Existing sessions are unchanged.\n'
