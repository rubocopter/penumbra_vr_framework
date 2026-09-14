# Penumbra VR Framework — Audit status

## Purpose

This document records the transition from the large technical audit into the current implementation workflow. It is not a replacement for the code; it is the bridge between findings, validation evidence and future implementation tasks.

## Audit baseline

The original audit was performed against `main` commit `73c70f0aad6fd2f33dc27983a44971ec44215f4a` with a clean working tree. Since then, the repository has continued evolving, so audit findings must be rechecked against the current HEAD before implementation.

## Current rule

Do not assume audit items are still pending. Before changing code:

1. Inspect current HEAD and relevant diffs.
2. Identify what has already been implemented.
3. Keep validated behavior intact.
4. Only address remaining gaps.

## Architecture constraints

- Overture Rework remains the behavioral reference, not a build dependency.
- Shared runtime behavior belongs in common systems.
- Game-specific ABI, RVAs and native engine ownership stay inside adapters/backends.
- Do not compensate behavioral issues by changing world scale or proven calibration formulas.
- Do not add duplicate ownership over existing native update boundaries.

## Validation vocabulary

Every feature must be tracked separately as:

- implemented
- host-tested
- live-tested
- headset-validated
- supported

A passing build or test suite does not automatically imply headset validation.

## Next implementation pass

The next Codex/Sol task must begin with repository inspection and reconcile this document with the current source state before modifying code.
