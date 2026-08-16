#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
FIXTURE_DIR="$REPO_ROOT/test/study/fixtures/simulator"
DECK_DIR="$REPO_ROOT/fs_/study/decks"

rm -rf "$DECK_DIR"
mkdir -p "$DECK_DIR"

# Keep the original root fixtures for regression coverage.
cp "$FIXTURE_DIR/basic.csv" "$DECK_DIR/basic.csv"
cp "$FIXTURE_DIR/empty.csv" "$DECK_DIR/empty.csv"
cp "$FIXTURE_DIR/invalid.csv" "$DECK_DIR/invalid.csv"
cp "$FIXTURE_DIR/long.csv" "$DECK_DIR/long.csv"
cp "$FIXTURE_DIR/review.csv" "$DECK_DIR/review.csv"
cp "$FIXTURE_DIR/utf8.csv" "$DECK_DIR/utf8.csv"

mkdir -p "$DECK_DIR/University/Networks"
mkdir -p "$DECK_DIR/University/Computer Science/Advanced Networks"
cp "$FIXTURE_DIR/university-networks.csv" "$DECK_DIR/University/networks.csv"
cp "$FIXTURE_DIR/tcp.csv" "$DECK_DIR/University/Networks/tcp.csv"
cp "$FIXTURE_DIR/routing.csv" "$DECK_DIR/University/Networks/routing.csv"
cp "$FIXTURE_DIR/broken.csv" "$DECK_DIR/University/Networks/broken.csv"
cp "$FIXTURE_DIR/long.csv" "$DECK_DIR/University/Computer Science/Advanced Networks/long-path.csv"
printf 'not a deck\n' > "$DECK_DIR/University/notes.txt"
printf 'temporary\n' > "$DECK_DIR/University/ignored.csv.tmp"

mkdir -p "$DECK_DIR/Personal"
cp "$FIXTURE_DIR/personal-networks.csv" "$DECK_DIR/Personal/networks.csv"

mkdir -p "$DECK_DIR/Languages/Spanish"
cp "$FIXTURE_DIR/german.csv" "$DECK_DIR/Languages/german.csv"
cp "$FIXTURE_DIR/basics.csv" "$DECK_DIR/Languages/Spanish/basics.csv"

mkdir -p "$DECK_DIR/Sprachen/Übungen"
cp "$FIXTURE_DIR/basics.csv" "$DECK_DIR/Sprachen/Übungen/vokabeln.csv"
mkdir -p "$DECK_DIR/EmptyFolder"

printf 'StudyPet simulator decks installed in %s\n' "$DECK_DIR"
