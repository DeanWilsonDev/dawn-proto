# Cimmerian — Feature Requests from the Dawn PoC

Gaps in [Cimmerian](https://github.com/DeanWilsonDev/cimmerian) noticed while writing
`dawn_tests` for the Dawn PoC. These are **feature requests / roadmap items**, not bugs —
the library works as-is; these are things that would have let the tests cover more, or read
more cleanly. (For the one outright *bug* found — the `IT` macro breaking on commas in
`{}` — see [`upstream-issues.md` → CIM-1](upstream-issues.md#cim-1--it--test-break-on-commas-inside--test-bodies--medium).)

Observed against Cimmerian commit `f76a38da5b2f6bb623a78e907a1570d1979d9ec6`.

The full current assertion set is:

```
ASSERT_TRUE   ASSERT_FALSE   ASSERT_EQUAL   ASSERT_NOT_EQUAL
REQUIRE_TRUE  REQUIRE_EQUAL
```

Priority: **High** = blocked real coverage in this PoC · **Medium** = clear ergonomic win ·
**Low** = nice-to-have.

---

## CFR-1 — `ASSERT_NEAR` / `REQUIRE_NEAR` for floating-point comparison · High

**The gap.** The only equality assertion is `ASSERT_EQUAL`, which compares with exact `==`.
There is no tolerance-based comparison, so any value produced by floating-point arithmetic
cannot be asserted reliably.

**How it bit the Dawn PoC.** To stay on the exact-`==` path, the serialisation tests were
written using only values that round-trip exactly through JSON and IEEE-754 (`100.0`,
`200.0`, `400.0`, `800.0`, `32.0`, …). More importantly, **two pieces of computed-float
logic were left untested** because there was no way to assert them within a tolerance:

- `EditorApplication::FrameEntities` — the camera fit/centre math (computes a zoom and an
  offset from a bounding box). Its result is inherently non-exact.
- The 0.5px "dead-zone" in the move-commit logic — naturally expressed as
  `|delta| < epsilon`, which wants a near-comparison to test.

With `ASSERT_NEAR` these would have been straightforward unit tests; without it they were
verified only by eye at runtime.

**Suggested API.**
```cpp
ASSERT_NEAR(actual, expected, tolerance);   // |actual - expected| <= tolerance; fail + continue
REQUIRE_NEAR(actual, expected, tolerance);  // ... fail + halt the test
```
On failure the diff should print actual, expected, the absolute difference, and the
tolerance. Consider a sensible default tolerance overload (`ASSERT_NEAR(a, b)` using a small
epsilon) for the common case.

---

## CFR-2 — Exception assertions: `ASSERT_THROWS` / `ASSERT_NO_THROW` · High

**The gap.** There is no way to assert that an expression does or does not throw, nor to
assert on the thrown type.

**How it bit the Dawn PoC.** Amanuensis' typed accessors (`AsString`, `AsInteger`,
`AsDouble`) **throw** `TypeMismatchError` on a wrong-typed node, and `Get`/`At` throw on a
missing key / out-of-range index. `SceneSerialiser` is written defensively specifically to
*avoid* throwing on malformed input. The natural test — "deserialising malformed JSON does
not throw" — could only be expressed indirectly (assert the function returns `false` and
leaves defaults), rather than directly asserting the no-throw contract. Conversely, there
was no clean way to assert that a raw `value.AsString()` on a non-string *does* throw.

**Suggested API.**
```cpp
ASSERT_THROWS(expr);                 // fails if expr does not throw
ASSERT_THROWS_AS(expr, ExceptionT);  // fails unless expr throws ExceptionT (or derived)
ASSERT_NO_THROW(expr);               // fails if expr throws
// REQUIRE_ variants that halt the test on failure
```

---

## CFR-3 — Complete the `REQUIRE_` family: `REQUIRE_FALSE`, `REQUIRE_NOT_EQUAL` · Medium

**The gap.** `REQUIRE_TRUE` and `REQUIRE_EQUAL` exist, but there is no `REQUIRE_FALSE` or
`REQUIRE_NOT_EQUAL`. The `ASSERT_` family is symmetric (`TRUE`/`FALSE`/`EQUAL`/`NOT_EQUAL`);
the `REQUIRE_` family is not.

**How it bit the Dawn PoC.** Where a fail-fast negative precondition was wanted — e.g.
"this entity id must *not* already exist before we proceed" or "the document must *not* be
dirty here" — the test had to fall back to `ASSERT_FALSE` (which continues after failing,
then runs assertions that assume the precondition held and cascade into noise).

**Suggested API.** Mirror the existing `REQUIRE_` macros:
```cpp
REQUIRE_FALSE(cond);
REQUIRE_NOT_EQUAL(a, b);
```

---

## CFR-4 — Substring / matcher assertions for strings · Medium

**The gap.** Strings can only be compared for full equality. There is no "contains",
"starts-with", or matcher-style assertion.

**How it would have helped.** Several Dawn behaviours produce *messages* or *generated*
strings where only a part is meaningful: command `Describe()` text (`"Place platform
(platform_1)"`), auto-generated entity names (`type + "_" + counter`), and parser error
messages. Testing those needs either brittle full-string equality or a substring check.
A UUID format check (`GenerateUuidV4` → `8-4-4-4-12` hex) similarly wants a pattern match
rather than equality.

**Suggested API.**
```cpp
ASSERT_CONTAINS(haystack, needle);
ASSERT_STARTS_WITH(str, prefix);
// optionally ASSERT_MATCHES(str, regex) for format checks
```

---

## CFR-5 — Parameterised / data-driven tests · Low

**The gap.** Each case is a separate `IT` block; there is no built-in way to run one body
over a table of inputs.

**How it would have helped.** Repetitive cases — the three `ParseHexColor` inputs, placing
each of the three schema entity types, round-tripping several entities — were written (or
would be written) as near-duplicate `IT`s. A data-driven form would cut the duplication and
make the matrix of cases obvious.

**Suggested API (illustrative).**
```cpp
IT_EACH("parses hex colour", ({ {"#000000", 0,0,0}, {"#4A90D9", 74,144,217} }),
        [](auto testCase) { /* ... */ });
```
(Exact shape is open; even a documented "loop inside one `IT` and assert per row" pattern
would help, though per-row failure reporting is the real value.)

---

## Summary checklist

- [ ] **CFR-1** `ASSERT_NEAR` / `REQUIRE_NEAR` (float tolerance) — *High; blocked camera + dead-zone coverage*
- [ ] **CFR-2** `ASSERT_THROWS` / `ASSERT_THROWS_AS` / `ASSERT_NO_THROW` — *High; the deps throw*
- [ ] **CFR-3** `REQUIRE_FALSE` / `REQUIRE_NOT_EQUAL` — *Medium; family symmetry*
- [ ] **CFR-4** string `CONTAINS` / `STARTS_WITH` / `MATCHES` — *Medium*
- [ ] **CFR-5** parameterised / data-driven tests — *Low*

Related bug (separate from these features): **CIM-1** — make the `IT`/hook macros variadic
so test bodies may contain top-level commas. See
[`upstream-issues.md`](upstream-issues.md#cim-1--it--test-break-on-commas-inside--test-bodies--medium).
