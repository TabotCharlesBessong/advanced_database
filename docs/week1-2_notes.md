# Weeks 1 & 2 Research and Design Notes

## Week 1: Database Theory Fundamentals

### Relational Algebra and Calculus
- Reviewed relational algebra operators (selection, projection, join, union, difference, Cartesian product).
- Noted that queries will be internally represented using relational algebra trees.

### Normalization Principles
- Studied 1NF, 2NF, 3NF, BCNF.
- Decided to support schemas in 3NF by default; enforcement will be via catalog checks.

### SQL Standards
- Targeting a small ANSI SQL subset initially: `CREATE`, `DROP`, `SELECT`, `INSERT`, `UPDATE`, `DELETE`.
- Supporting basic expressions and simple WHERE clauses.

### ACID and Transaction Theory
- Read up on ACID properties; commit to WAL-based durability and MVCC.

## Week 2: Architecture Design

- Created high-level diagrams (see `architectural_design.md`).
- Chose layered architecture: React UI -> Node.js API -> C++ engine -> file storage.
- Defined component responsibilities and initial interfaces.

## Action Items
1. Finalize decision on page size (4KB).
2. Sketch out initial class names for engine (`PageManager`, `BufferPool`, etc.).
3. Create project skeleton (directories, build files).

---
