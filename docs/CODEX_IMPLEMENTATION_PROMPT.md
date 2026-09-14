# Codex implementation prompt

## Context

Continue development of Penumbra VR Framework from the current repository state.

Before modifying code:

- Inspect current HEAD.
- Read `AGENTS.md`, `docs/AUDIT_STATUS.md`, `ROADMAP.md` and architecture documentation.
- Do not repeat the previous audit blindly.
- Reconcile existing implementation with previous findings.

## Core objective

Complete the framework by preserving proven behavior and closing only remaining gaps between the shared runtime architecture and per-game integrations.

## Rules

- Overture behavior is the reference implementation where already validated.
- Do not rewrite working systems for architectural cleanliness alone.
- Do not move game-specific binary knowledge into generic runtime code.
- Do not invent RVAs, offsets or native contracts without evidence.
- Keep validation states explicit.

## Priority order

1. Verify current ownership boundaries and lifecycle.
2. Complete remaining tracking/body/render consistency work.
3. Complete shared posture and movement policy extraction where evidence exists.
4. Complete interaction/contact behavior using Rework as behavioral reference.
5. Improve validation coverage.

## Expected workflow

For each change:

1. Identify current behavior.
2. Identify the smallest missing contract.
3. Implement without regressing validated paths.
4. Add or update tests.
5. Update documentation with the new validation state.

The goal is not to recreate the audit plan. The goal is to finish the framework from its actual current state.
