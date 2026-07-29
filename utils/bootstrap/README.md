# MrDocs Bootstrap Tool

A Python tool for setting up the MrDocs development environment. It handles dependency installation, CMake preset generation, and IDE configuration.

## Quick Start

From the MrDocs root directory:

```bash
# Interactive setup (prompts for options)
python bootstrap.py

# Non-interactive with defaults
python bootstrap.py -y

# Dry run (show what would be done)
python bootstrap.py --dry-run
```

## Usage

```bash
python bootstrap.py [options]
```

### Common Options

| Option | Description |
|--------|-------------|
| `-y`, `--non-interactive` | Accept all defaults without prompting |
| `--dry-run` | Show what would be done without executing |
| `--build-type TYPE` | Set build type: `Release`, `Debug`, `RelWithDebInfo`, `MinSizeRel` |
| `--preset NAME` | CMake preset name |
| `--cc PATH` | C compiler path |
| `--cxx PATH` | C++ compiler path |
| `--sanitizer TYPE` | Enable sanitizer: `address`, `undefined`, `thread`, `memory` |
| `--build-tests` | Build tests (default) |
| `--no-build-tests` | Don't build tests |
| `--list-recipes` | List available dependency recipes |
| `--verbose` | Verbose output |
| `--help` | Show all options |

### Examples

```bash
# Debug build with Clang
python bootstrap.py --build-type Debug --cc clang --cxx clang++

# Release build with address sanitizer
python bootstrap.py --build-type Release --sanitizer address

# List available recipes
python bootstrap.py --list-recipes

# Only install specific recipes
python bootstrap.py --recipe-filter llvm,libxml2
```

## Project Structure

```
utils/bootstrap/
├── src/
│   ├── __init__.py          # Package metadata
│   ├── __main__.py          # Entry point
│   ├── installer.py         # Main orchestrator
│   ├── core/                # Core utilities
│   │   ├── ui.py            # Console output formatting
│   │   ├── platform.py      # Platform detection
│   │   ├── options.py       # Configuration dataclass
│   │   ├── filesystem.py    # File operations
│   │   ├── process.py       # Command execution
│   │   └── prompts.py       # User input handling
│   ├── tools/               # Tool detection
│   │   ├── detection.py     # Generic tool finding
│   │   ├── compilers.py     # Compiler probing
│   │   ├── ninja.py         # Ninja installation
│   │   ├── visual_studio.py # VS detection (Windows)
│   │   └── java.py          # Java detection
│   ├── recipes/             # Dependency management
│   │   ├── schema.py        # Recipe dataclasses
│   │   ├── loader.py        # Recipe file loading
│   │   ├── fetcher.py       # Source fetching
│   │   ├── builder.py       # Build execution
│   │   └── archive.py       # Archive extraction
│   ├── presets/             # CMake presets
│   │   └── generator.py     # Preset generation
│   └── configs/             # IDE configurations
│       ├── run_configs.py   # Config orchestration
│       ├── clion.py         # CLion XML configs
│       ├── vscode.py        # VSCode JSON configs
│       ├── visual_studio.py # VS JSON configs
│       └── pretty_printers.py # Debugger configs
├── tests/                   # Unit tests
│   ├── test_platform.py
│   ├── test_filesystem.py
│   ├── test_options.py
│   ├── test_recipes.py
│   ├── test_presets.py
│   └── test_ui.py
└── README.md
```

## Running Tests

```bash
cd utils/bootstrap

# Run all tests
python -m unittest discover -s tests/ -v

# Run specific test file
python -m unittest tests.test_filesystem -v

# Run specific test class
python -m unittest tests.test_recipes.TestTopoSortRecipes -v

# Run specific test method
python -m unittest tests.test_presets.TestGetParentPresetName.test_debug_returns_debug -v
```

## What It Does

1. **Checks required tools** - Verifies git, cmake, python are available
2. **Sets up compilers** - Detects or prompts for C/C++ compilers
3. **Configures build options** - Build type, sanitizers, test building
4. **Installs dependencies** - Fetches and builds third-party libraries (LLVM, libxml2, etc.)
5. **Creates CMake presets** - Generates `CMakeUserPresets.json`
6. **Generates IDE configs** - Creates run configurations for CLion, VSCode, Visual Studio

## Recipes

Recipes define how to fetch and build third-party dependencies. They're JSON files in `utils/bootstrap/recipes/`.

List available recipes:
```bash
python bootstrap.py --list-recipes
```

## Environment Variables

| Variable | Description |
|----------|-------------|
| `BOOTSTRAP_FORCE_COLOR` | Force colored output |
| `BOOTSTRAP_FORCE_EMOJI` | Force emoji in output |
| `BOOTSTRAP_PLAIN` | Disable all formatting |
| `NO_COLOR` | Disable colors (standard) |

## Troubleshooting

### Clean rebuild
```bash
python bootstrap.py --clean --force
```

### Verbose output
```bash
python bootstrap.py --verbose --debug
```

### Skip dependency building
```bash
python bootstrap.py --skip-build
```
