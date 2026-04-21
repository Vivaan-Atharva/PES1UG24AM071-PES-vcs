# Building PES-VCS 

## Objective

Build the initial parts of a local version control system that supports:

* Content-addressable object storage
* Tree object serialization
* Deterministic directory snapshots

Current progress includes:

* Phase 1: Object Storage Foundation
* Phase 2: Tree Objects

---

## Prerequisites

```bash
sudo apt update
sudo apt install -y gcc build-essential libssl-dev
```

---

## Building

```bash
make          # Build the pes binary
make all      # Build pes + test binaries
make clean    # Remove build artifacts
```

---

## Repository Structure

```text
.pes/
├── objects/
│   ├── 02/
│   └── ab/
├── refs/
│   └── heads/
└── HEAD
```

Objects are stored using SHA-256 hashes.

The first two hex digits become the directory name.

Example:

```text
021657cde88428337031b3a946b7d1253ec80a7893aaa22dc4c2332ac201b2d3
```

Stored as:

```text
.pes/objects/02/1657cde88428337031b3a946b7d1253ec80a7893aaa22dc4c2332ac201b2d3
```

---

# Phase 1: Object Storage Foundation

## Concepts Covered

* Content-addressable storage
* SHA-256 hashing
* Atomic writes
* Object deduplication
* Directory sharding

## Implemented Functions

In `object.c`:

* `object_write()`
* `object_read()`

### object_write()

This function:

1. Builds an object header:

```text
blob <size>\0
```

2. Concatenates the header and raw data
3. Computes the SHA-256 hash
4. Creates the correct object path
5. Stores the object atomically

### object_read()

This function:

1. Reads the object from disk
2. Parses the object header
3. Verifies the SHA-256 hash
4. Returns the original object data

---

## Phase 1 Testing

Build and run:

```bash
make test_objects
./test_objects
```



Expected checks:

* Blob write/read roundtrip
* Deduplication
* Integrity verification

Useful command:

```bash
find .pes/objects -type f
```



---

# Phase 2: Tree Objects

## Concepts Covered

* Directory representation
* Tree serialization
* File modes
* Deterministic ordering
* Tree object storage

## Implemented Functions

In `tree.c`:

* `tree_serialize()`
* `tree_parse()`
* `tree_from_index()`

## Tree Format

Each tree entry contains:

```text
<mode> <hash> <name>
```

Example:

```text
100644 blob README.md
100755 blob build.sh
040000 tree src
```

Mode meanings:

* `100644` → Regular file
* `100755` → Executable file
* `040000` → Directory

---

## Deterministic Serialization

Tree entries are sorted alphabetically before serialization.

Example:

```text
README.md
build.sh
src
```

This guarantees that the same logical tree always produces the same hash.

---

## Phase 2 Testing

Build and run:

```bash
make test_tree
./test_tree
```




Expected checks:

* Serialize/parse roundtrip
* Deterministic serialization
* Tree object storage

Example output:

```text
Serialized tree: 139 bytes
Stored tree object: 021657cde88428337031b3a946b7d1253ec80a7893aaa22dc4c2332ac201b2d3
PASS: tree serialize/parse roundtrip
PASS: tree deterministic serialization
```

Example stored object path:

```text
.pes/objects/02/1657cde88428337031b3a946b7d1253ec80a7893aaa22dc4c2332ac201b2d3
```

To inspect the raw binary object:

```bash
xxd .pes/objects/02/1657cde88428337031b3a946b7d1253ec80a7893aaa22dc4c2332ac201b2d3 | head -20
```



---

# Files Modified

| File          | Purpose                        |
| ------------- | ------------------------------ |
| `object.c`    | Object store implementation    |
| `tree.c`      | Tree serialization and parsing |
| `test_tree.c` | Tree storage testing           |
| `Makefile`    | Build rules for test binaries  |

---

# Current Status

Completed:

* Phase 1
* Phase 2



# Useful Commands

```bash
make clean
make test_objects
make test_tree
find .pes/objects -type f
```

---

# Author

Vivaan
PES1UG24AM071

