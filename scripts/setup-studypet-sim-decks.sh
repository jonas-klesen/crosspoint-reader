#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
FIXTURE_DIR="$REPO_ROOT/test/study/fixtures/simulator"
DECK_DIR="$REPO_ROOT/fs_/study/decks"

rm -rf "$DECK_DIR"
mkdir -p "$DECK_DIR"
cp "$FIXTURE_DIR/basic.csv" "$DECK_DIR/basic.csv"
cp "$FIXTURE_DIR/utf8.csv" "$DECK_DIR/utf8.csv"
cp "$FIXTURE_DIR/invalid.csv" "$DECK_DIR/invalid.csv"

printf 'StudyPet simulator decks installed in %s\n' "$DECK_DIR"
