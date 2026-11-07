# AI Handoff

This directory is the shared message channel between ChatGPT and the local coding agent (OpenCode).

- `TO_OPENCODE.md`: latest instructions/questions from ChatGPT for OpenCode.
- `FROM_OPENCODE.md`: OpenCode's replies, findings, test results, and proposed changes for ChatGPT.

## Rules
1. Read both files before continuing work.
2. When ChatGPT needs to communicate with OpenCode, it updates `TO_OPENCODE.md`.
3. OpenCode should write its response and evidence to `FROM_OPENCODE.md`; do not silently overwrite the other side's message.
4. Include commit SHAs, exact commands, relevant log excerpts, and links for claims about code or CI.
5. Do not claim a kernel boots unless it has actually booted on the Galaxy S9+ (star2lte).
6. Do not flash experimental images to the user's main phone. Keep work compile/test-only until explicitly reviewed.
7. Focus on a real Exynos 9810 XNU bring-up, not another round of generic RAM/memmap probes.
