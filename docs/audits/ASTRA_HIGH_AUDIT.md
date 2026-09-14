# Astra High audit — preserved handoff

This directory preserves the complete Astra High technical audit that was delivered for Penumbra VR Framework before the post-audit implementation work.

## Historical baseline

- Framework branch: `main`
- Audited commit: `73c70f0aad6fd2f33dc27983a44971ec44215f4a`
- Rework behavioral reference: `23c890f7dbd06b939be9951d282e6e948d9a6623`
- The audit itself made no source/documentation/configuration changes.

The audit is intentionally preserved as a historical snapshot. Several findings were subsequently implemented or narrowed by new evidence. **Do not use the historical plan as a current unchecked task list.** Read [`../AUDIT_STATUS.md`](../AUDIT_STATUS.md) for the current reconciliation and [`../IMPLEMENTATION_PLAN.md`](../IMPLEMENTATION_PLAN.md) for the active plan.

## Complete audit

The original handoff is split only to keep the repository documents readable. Read the parts in order; together they preserve the complete supplied audit, including the original Sol implementation prompt.

1. [`ASTRA_HIGH_AUDIT_01_BASELINE_ARCHITECTURE.md`](ASTRA_HIGH_AUDIT_01_BASELINE_ARCHITECTURE.md) — audit conclusion, baseline, validation state and architecture map.
2. [`ASTRA_HIGH_AUDIT_02_COMPARISON_ROOT_CAUSES.md`](ASTRA_HIGH_AUDIT_02_COMPARISON_ROOT_CAUSES.md) — Rework comparison, root causes R1–R7, problem clusters and workaround treatment.
3. [`ASTRA_HIGH_AUDIT_03_IMPLEMENTATION_PLAN.md`](ASTRA_HIGH_AUDIT_03_IMPLEMENTATION_PLAN.md) — original six-phase implementation plan.
4. [`ASTRA_HIGH_AUDIT_04_VALIDATION_ACCEPTANCE.md`](ASTRA_HIGH_AUDIT_04_VALIDATION_ACCEPTANCE.md) — affected files, do-not-touch constraints, automatic/live validation and acceptance criteria.
5. [`ASTRA_HIGH_AUDIT_05_SOL_PROMPT.md`](ASTRA_HIGH_AUDIT_05_SOL_PROMPT.md) — original implementation prompt prepared by Astra for Sol.

## Current continuation documents

Use these after reading the historical audit:

- [`../AUDIT_STATUS.md`](../AUDIT_STATUS.md) — finding-by-finding reconciliation against the post-audit repository state.
- [`../IMPLEMENTATION_PLAN.md`](../IMPLEMENTATION_PLAN.md) — current priority order and remaining evidence/implementation gates.
- [`../DESIGN_DECISIONS.md`](../DESIGN_DECISIONS.md) — invariants and decisions that should not be reopened without contradictory evidence.
- [`../CODEX_IMPLEMENTATION_PROMPT.md`](../CODEX_IMPLEMENTATION_PROMPT.md) — current Codex/Sol objective, written to start from the actual HEAD rather than replay the historical audit.

## Reading rule for future agents

Astra's audit explains **why** the intervention was designed. The current source and fresh evidence determine **what remains to be done**.

When the historical audit and current documentation differ, inspect current source/tests and exact-build/live evidence first, then update the current documents. Do not rewrite the preserved historical audit to make it look current.