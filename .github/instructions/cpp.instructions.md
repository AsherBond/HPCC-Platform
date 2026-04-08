---
applyTo:
  - "**/*.cpp"
  - "**/*.hpp"
  - "**/*.h"
---
# C++ Code Style and Development Instructions

## Important Formatting Rule: Trailing Whitespaces
- **NO TRAILING WHITESPACES**: This is a strict rule. Never introduce lines with trailing whitespaces anywhere in the code. Ensure any generated or modified code is completely free of spaces or tabs at the end of lines.

## Code Style (Essential Rules)
- Use Allman brace style, but allow single-line blocks to have no braces, unless nested.
- Use camelCase for variable names.
- Use `constexpr` over macros.
- Use `#pragma once` for header guards.
- Use `Owned<>` vs `Linked<>` for object ownership.
- Avoid default parameters (use method overloading instead).
- Use `%u` for unsigned integers, `%d` for signed integers.
- Complete style guide: `devdoc/StyleGuide.md`.

## Memory Management and Pointers
- Verify proper `Owned<>`/`Linked<>` usage.
- Check for memory/resource leaks in general.

## Thread Safety
- Look for race conditions, proper locking, and synchronization issues, especially in server components.

## HPCC Interface Architecture (Java-Style Macros)
- Use standard HPCC macros for inheritance to denote intent:
  - `interface`: Use to declare a pure virtual class (macro expands to `struct`).
  - `extends`: Use when one interface inherits from another (macro expands to `public`).
  - `implements`: Use when a concrete class implements an interface (macro expands to `public`).
- **Naming Conventions**:
  - Interfaces must be prefixed with `I` (e.g., `IMyService`).
  - Concrete class implementations should be prefixed with `C` (e.g., `CMyService : implements IMyService`).

## Classes & Initialization
- **Member vs Parameter Naming**: Use strict `camelCase`. When a parameter's sole purpose is to initialize a member variable of the same name (like in a setter or constructor), prefix the parameter with an underscore (e.g., assigning parameter `_count` to member `count`).

## Exception Handling
- Throw and catch HPCC native exceptions utilizing `IException`. Use `ThrowStringException(...)` or `MakeStringException(...)` formats instead of throwing raw `std::runtime_error` or `std::exception`.
