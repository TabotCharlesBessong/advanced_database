# Advanced Database System Learning Roadmap

## Purpose

This guide teaches a beginner how to learn the full stack of this project from start to finish:

- C++ database engine
- Node.js API layer
- React UI layer
- Testing, debugging, and architecture thinking

The goal is not just to follow code, but to build the same system independently.

---

## Learning Philosophy

Use this cycle for every concept:

1. Learn the concept
2. Implement a small version
3. Test it
4. Explain it in plain language
5. Refactor and improve

Three learning passes:

1. Pass 1: Make it work end to end
2. Pass 2: Understand why each layer exists
3. Pass 3: Rebuild from scratch with your own design decisions

---

## Prerequisites (Week 0)

Before starting the phased roadmap, make sure you are comfortable with:

1. SQL fundamentals: SELECT, WHERE, JOIN, GROUP BY, ORDER BY
2. C++ fundamentals: classes, pointers/references, RAII, file I/O
3. Node.js and Express fundamentals
4. React fundamentals: components, state, effects, API calls
5. Git basics and command-line workflow

Outcome:

- You can explain how a user action in a browser reaches database storage.

---

## Phase 1: System Orientation and Setup (Week 1)

### Objectives

1. Understand the layered architecture: Engine -> API -> UI
2. Build and run each layer
3. Identify entry points and execution flow

### Read First

- Project root README and planning docs
- Engine entry file and CMake setup
- API app/server setup
- UI Vite setup and app entry

### Deliverables

1. Local setup notes
2. A personal architecture diagram
3. A list of major components and responsibilities

### Checkpoint

- You can describe an end-to-end request path from UI action to engine result.

---

## Phase 2: Storage Engine Foundations (Weeks 2-4)

### Objectives

1. Understand page-based storage
2. Understand file manager and buffer pool behavior
3. Learn how tuples/records are physically stored

### Topics

1. Page abstraction and serialization
2. File manager responsibilities
3. Buffer pool and replacement policy
4. Slotted-page layout
5. Table heap insertion and retrieval

### Practice Tasks

1. Insert rows and inspect persisted files
2. Simulate restart and verify data durability
3. Track page allocation and free-space behavior

### Deliverables

1. Notes on page lifecycle
2. Mini test cases for insert/read persistence
3. Diagram of record storage layout

### Checkpoint

- You can explain how a row is transformed from in-memory values to bytes on disk.

---

## Phase 3: Catalog and Schema System (Weeks 5-6)

### Objectives

1. Understand schema definitions and metadata
2. Understand data typing and layout decisions
3. Connect table creation to storage structures

### Topics

1. Table metadata storage
2. Column types and constraints
3. Record layout rules
4. Catalog lookup and table discovery

### Practice Tasks

1. Create multiple table schemas
2. Validate type mismatches and schema errors
3. Add a new type and test serialization

### Deliverables

1. Schema-to-layout mapping notes
2. Validation rule checklist
3. Small schema test suite

### Checkpoint

- You can explain where and how table structure is stored and used during query execution.

---

## Phase 4: SQL Front End (Lexer, Parser, AST) (Weeks 7-8)

### Objectives

1. Parse SQL text into structured internal representation
2. Understand grammar rules and parse errors
3. Build confidence in language front-end design

### Topics

1. Tokenization (keywords, identifiers, literals)
2. SQL grammar basics
3. Recursive descent parsing
4. AST node design
5. Syntax error handling

### Practice Tasks

1. Parse CREATE, INSERT, SELECT
2. Add one grammar feature (for example LIMIT or aliases)
3. Improve parser error messages

### Deliverables

1. AST diagrams for sample queries
2. Parser tests for valid/invalid SQL
3. Notes on grammar tradeoffs

### Checkpoint

- You can manually walk from SQL text to AST tree and explain each node.

---

## Phase 5: Query Planning and Execution (Weeks 9-11)

### Objectives

1. Convert AST into execution plans
2. Execute scan/filter/project pipelines
3. Understand operator composition

### Topics

1. Logical planning vs physical planning
2. Volcano/iterator style execution
3. Scan, filter, project operators
4. Predicate evaluation
5. Result materialization

### Practice Tasks

1. Trace execution for simple SELECT queries
2. Add support for one additional operator
3. Measure operator-level performance

### Deliverables

1. Query-plan walkthrough documents
2. Operator unit tests
3. Execution traces for sample queries

### Checkpoint

- You can explain why query plans are trees and how tuples flow through operators.

---

## Phase 6: Advanced Query Features (Weeks 12-14)

### Objectives

1. Understand analytical query components
2. Implement and test richer SQL capabilities

### Topics

1. JOIN strategies (nested loop at minimum)
2. GROUP BY and aggregations (COUNT, SUM, AVG)
3. HAVING filtering
4. ORDER BY and sorting

### Practice Tasks

1. Write tests for grouped and joined queries
2. Compare naive vs optimized execution behavior
3. Handle nulls and edge cases

### Deliverables

1. Query behavior matrix by feature
2. Regression tests for advanced SQL
3. Benchmark notes for heavy queries

### Checkpoint

- You can explain the full execution flow of a JOIN + GROUP BY query.

---

## Phase 7: Indexing and Performance (Weeks 15-16)

### Objectives

1. Learn indexing internals
2. Integrate index usage into query execution
3. Measure and reason about performance gains

### Topics

1. B-Tree structure and node operations
2. Point lookup vs range scan
3. Index maintenance on writes
4. Planner/index selection basics

### Practice Tasks

1. Build index and compare scan timings
2. Test index consistency after updates/deletes
3. Add explain-style diagnostics

### Deliverables

1. Before/after benchmark table
2. Index correctness tests
3. Notes on index tradeoffs

### Checkpoint

- You can justify when an index helps and when it does not.

---

## Phase 8: Transactions and Concurrency (Weeks 17-19)

### Objectives

1. Understand correctness under concurrency
2. Learn transaction boundaries and recovery concepts

### Topics

1. Transaction lifecycle: begin, commit, rollback
2. Locking basics and conflict handling
3. MVCC concepts
4. WAL/recovery fundamentals

### Practice Tasks

1. Reproduce race conditions in controlled tests
2. Validate rollback behavior
3. Simulate crashes and recovery flows

### Deliverables

1. Isolation anomaly examples
2. Concurrency test scripts
3. Recovery scenario notes

### Checkpoint

- You can explain atomicity, consistency, isolation, and durability in this system.

---

## Phase 9: Engine to API Integration (Weeks 20-21)

### Objectives

1. Build a robust bridge from Node.js to engine
2. Standardize request/response and error contracts

### Topics

1. Process invocation or client communication model
2. Input sanitization and validation boundaries
3. Structured JSON responses
4. Error propagation and observability

### Practice Tasks

1. Add endpoint for one new query behavior
2. Harden error handling for malformed SQL
3. Add trace logs for request lifecycle

### Deliverables

1. API contract document
2. Integration tests for engine responses
3. Error taxonomy and handling guide

### Checkpoint

- You can diagnose whether a failure came from UI, API, parser, planner, or storage.

---

## Phase 10: API Layer and Testing Discipline (Weeks 22-23)

### Objectives

1. Build production-quality endpoints
2. Test behavior thoroughly

### Topics

1. Express route design
2. Validation and authentication basics
3. Prepared statements and safe patterns
4. Endpoint testing and mocks

### Practice Tasks

1. Add tests for all happy and error paths
2. Enforce input validation centrally
3. Document API via Swagger or equivalent

### Deliverables

1. Complete endpoint test suite
2. API docs examples
3. Reliability checklist

### Checkpoint

- You can create and test new endpoints confidently without regressions.

---

## Phase 11: UI Query Workbench (Weeks 24-25)

### Objectives

1. Build a usable interface for SQL operations
2. Manage state and asynchronous API interactions

### Topics

1. Query editor UX
2. Results table rendering
3. Error and loading states
4. Query history/favorites
5. Basic performance display

### Practice Tasks

1. Implement run-query flow from UI to API
2. Add robust empty/error state handling
3. Persist user preferences locally

### Deliverables

1. UI interaction map
2. Component state diagrams
3. End-to-end manual test checklist

### Checkpoint

- You can explain exactly how UI state transitions during query execution.

---

## Phase 12: Rebuild Challenge (Weeks 26-28)

### Objective

Rebuild a minimal version from scratch without copying implementation details directly.

### Rules

1. New repository
2. Re-implement engine core, API, and a minimal UI
3. Add one custom feature not in the original
4. Write architecture rationale for your design decisions

### Final Deliverables

1. End-to-end demo
2. Architecture document
3. Performance comparison report
4. Test and reliability report

### Mastery Checkpoint

- You can independently design and implement a mini database stack.

---

## Weekly Learning Cadence

Use this pattern every week:

1. Day 1: Read and map control flow
2. Day 2: Reimplement one core piece
3. Day 3: Write tests and edge cases
4. Day 4: Debug a deliberate failure scenario
5. Day 5: Summarize what you learned and teach it back

---

## Evaluation Rubric (Self-Assessment)

Score each area from 1 to 5:

1. Storage internals understanding
2. SQL parsing and AST understanding
3. Query planning and execution understanding
4. Performance tuning and indexing understanding
5. Transaction/concurrency understanding
6. API design and test quality
7. UI data flow and state management
8. End-to-end debugging speed

Target:

- Reach at least 4/5 in all areas before declaring full readiness.

---

## Recommended Learning Outputs

Produce these artifacts as you progress:

1. Architecture diagrams
2. Feature design notes
3. Test plans and regression checklists
4. Performance benchmark logs
5. Postmortems for bugs you fixed

These outputs build deep understanding and make your growth measurable.

---

## Final Outcome

By the end of this roadmap, you should be able to:

1. Design a database architecture from first principles
2. Implement storage, parser, planner, and executor components
3. Expose functionality safely through an API
4. Build a UI that behaves reliably under real usage
5. Debug and optimize the full stack independently
