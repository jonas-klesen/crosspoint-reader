#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
FIXTURE_DIR="$REPO_ROOT/test/study/fixtures/simulator"
DECK_DIR="$REPO_ROOT/fs_/study/decks"

rm -rf "$DECK_DIR"
mkdir -p "$DECK_DIR"
cp "$FIXTURE_DIR/basic.csv" "$DECK_DIR/basic.csv"
cp "$FIXTURE_DIR/empty.csv" "$DECK_DIR/empty.csv"
cp "$FIXTURE_DIR/invalid.csv" "$DECK_DIR/invalid.csv"
cp "$FIXTURE_DIR/long.csv" "$DECK_DIR/long.csv"
cp "$FIXTURE_DIR/review.csv" "$DECK_DIR/review.csv"
cp "$FIXTURE_DIR/utf8.csv" "$DECK_DIR/utf8.csv"

printf 'StudyPet simulator decks installed in %s\n' "$DECK_DIR"
