# Memory-Efficient Versioned File Indexer

A C++ command-line indexing engine that processes large text/log files using fixed-size buffered I/O and builds case-insensitive word-frequency indexes for multiple file versions.

This was developed as a CS253 course mini project at IIT Kanpur, with emphasis on memory-efficient streaming, object-oriented design, and query processing.

## Features

- Streams large files using a fixed buffer of 256 KB to 1024 KB
- Builds word-frequency indexes without loading complete files into memory
- Handles words split across buffer boundaries
- Supports multiple named versions in one execution
- Provides three query modes:
  - word-frequency lookup
  - top-K frequent words
  - cross-version frequency difference
- Uses modular OOP design with inheritance, runtime polymorphism, templates, function overloading, and exception handling

## Project Structure

```text
.
├── README.md
├── src/
│   └── main.cpp
└── sample_data/
    ├── version1.txt
    └── version2.txt
```

## Build

```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic src/main.cpp -o analyzer
```

## Usage

### Word Count Query

```bash
./analyzer --file sample_data/version1.txt --version v1 --buffer 256 --query word --word error
```

### Top-K Query

```bash
./analyzer --file sample_data/version1.txt --version v1 --buffer 256 --query top --top 5
```

### Difference Query

```bash
./analyzer --file1 sample_data/version1.txt --version1 v1 --file2 sample_data/version2.txt --version2 v2 --buffer 256 --query diff --word error
```

Difference is computed as:

```text
frequency(version2, word) - frequency(version1, word)
```

## Architecture

```text
Input File
   ↓
BufferedFileReader
   ↓
WordTokenizer
   ↓
VersionedWordIndex
   ↓
Query Engine
```

### Core Components

- `BufferedFileReader`: reads fixed-size chunks from disk.
- `WordTokenizer`: extracts lowercase alphanumeric tokens and preserves partial words across chunk boundaries.
- `VersionedWordIndex`: maintains `version -> word -> frequency` mappings and supports overloaded indexing methods.
- `Query`: abstract base class for polymorphic query execution.
- `WordQuery`, `TopKQuery`, `DiffQuery`: derived query classes implementing runtime dispatch.

## Complexity

Let `N` be the number of tokens and `M` be the number of unique words in an indexed version.

| Operation | Complexity |
|---|---:|
| Index construction | `O(N)` |
| Word lookup | `O(1)` average |
| Difference query after indexing | `O(1)` average |
| Top-K query | `O(M log M)` |

Space usage is `O(M)` for the word-frequency index plus `O(B)` for the fixed-size buffer, where `B` is bounded between 256 KB and 1024 KB.

## Notes

- Words are treated as contiguous alphanumeric sequences.
- Matching is case-insensitive.
- The index is in-memory and not persisted across program runs.
- The implementation is intended for text/log-style datasets.
