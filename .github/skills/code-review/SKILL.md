---
name: code-review
description: >
  Code review checklist for the HPCC Platform. Use when reviewing C++ code changes,
  examining components for issues, or checking best practices. Covers memory
  management, thread safety, style compliance, security, API compatibility, and correctness.
---

# HPCC Platform Code Review Guidelines

## Review Philosophy
- The goal is to catch bugs, design problems, security issues, and readability concerns.
- Consider interactions between components, e.g. Dali, Thor, and Roxie components.

## Critical Quality Checks & Checklist

### 1. Memory Management
- **Owned<> vs Linked<>**: `Owned<X>` takes ownership of a new/returned pointer; `Linked<X>` shares ownership. Verify proper use.
- Check for memory/resource leaks in general. Are exceptions properly handled so resources are released?
- **queryFoo()** returns are NOT linked — caller must `Link()` if retaining beyond guaranteed lifetime.
- **getFoo()** returns ARE linked — assign to `Owned<>` or return directly.
- Are `CInterface`-derived objects properly release-counted?

### 2. Thread Safety
- Look for race conditions, proper locking, and synchronization issues, especially in server components.
- Are shared variables protected by critical sections or atomics? Watch for TOCTOU (time-of-check-time-of-use).
- Check for potential deadlocks (lock ordering).
- Are `CriticalBlock`, `ReadLockBlock`, `WriteLockBlock` scoped correctly?

### 3. Code Style (Essential Rules)
- Adherence to coding standards in `devdoc/StyleGuide.md`.
- **Braces**: Allman style — `{` and `}` on their own line. Single-line blocks may omit braces unless nested.
- **Naming**: `CamelCase` for classes, `camelCase` for variables/functions/methods.
- **No trailing whitespace**.
- Prefer `constexpr` over macros. Avoid default parameters (use method overloading instead).
- Format specifiers: `%u` for unsigned, `%d` for signed.

### 4. Error Handling & Correctness
- Consistent error reporting and logging throughout the codebase.
- Edge cases: empty inputs, null pointers, integer overflow, off-by-one.
- Are exceptions caught at the right level? Do they properly release resources?

### 5. Security & API Compatibility
- Validate input sanitization, authentication, and authorization mechanisms.
- Ensure changes don't break existing client interfaces or backward compatibility.
- Are public interface changes minimized and documented?
- SQL/command injection vectors?

### 6. Efficiency
- Consider effects on query execution and distributed data throughput.
- Unnecessary copies of large objects?
- String operations in hot paths (prefer `StringBuffer` over repeated `std::string` concatenation)?

### 7. Component-Specific Concerns
- **Dali**: Distributed metadata — watch for consistency, replication, and transaction issues.
- **Thor**: Batch processing — watch for data partitioning, memory limits, and spill-to-disk handling.
- **Roxie**: Real-time queries — watch for latency, caching, and query lifecycle management.
- **ESP**: Web services — watch for authentication, input validation, and CORS/CSRF.
- **ECL compiler**: Language changes — verify backward compatibility with existing ECL code.

## Review Questions to Consider
- Are there any efficiency concerns or performance bottlenecks?
- Is the code thread-safe and properly synchronized?
- Could the code be refactored to improve maintainability and reuse?
- Are there any memory or resource leaks?
- Does this change maintain backward compatibility?
- Are error conditions properly handled and logged?
- Is the change properly tested with appropriate test coverage?
