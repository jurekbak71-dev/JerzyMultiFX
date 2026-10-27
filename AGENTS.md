# Jerzy MultiFX

Use this existing checkout; do not create a worktree unless the user explicitly asks. Source repositories in /workspace are read-only references. All project changes belong here. Do not reset or force-push other repositories.

Read STATUS.md and docs/REQUIREMENTS_PL.md before implementation. STATUS.md must distinguish executed tests, unrun checks, actual binaries, and physical hardware validation. Never call a source archive an installer or an x86 binary an ARM build.

The shared core must not depend on a GUI or audio backend. Audio callbacks must not allocate, lock, access files or network, or initialize lazy metadata. Keep stable parameter keys and VST IDs; append parameters or provide migration. State includes embedded IR and CC maps.

Builds and intermediate dependencies belong in .build; packages in dist. See scripts/build.sh. Test native DSP, Windows VST3/GUI, ARM64/QEMU, HTTP/JACK integration, BLE/LCD logic and package contents. Preserve assertions and cryptographic/TLS verification.

Reference images remain private research material, not binary-package contents. Source provenance and licenses live in docs/source-manifest.json and docs/SOURCE_AUDIT.md. Public distribution requires resolving the explicitly identified missing original licenses. Never infer redistribution rights from a screenshot.

Physical FL Studio, Raspberry Pi/USB audio, BLE radio/iOS and LCD need hardware evidence; do not fabricate it. Use prerelease labels until the applicable hardware checklist passes. Do not format cards or alter an existing audio installation without explicit hardware-owner authorization.
