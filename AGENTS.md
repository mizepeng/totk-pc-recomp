# TotK Recomp

Static recompilation of *Tears of the Kingdom* (Switch, ARM64) into a native Windows x86-64 exe with a Vulkan renderer. Design: [docs/architecture.md](docs/architecture.md). Toolchain, build commands and tests: [docs/building.md](docs/building.md).

## Game content boundary

This repository is public. Game data lives outside it and every derived file stays in ignored folders.

- The user's game and key paths are in `config.local.toml` (ignored). Pass the keys file path to tools; the tools read it themselves. Key values stay out of your output, logs, commands and files.
- Everything extracted or generated goes under `build/` or `generated/`.
- Every file the code generator emits carries the marker defined as `GENERATED_MARKER` in `tools/check_content.py`.
- Run `python tools/check_content.py --staged` before proposing a commit. It is the same check CI runs.

## Reusing code

`refs/` holds the upstream checkouts (`tools/fetch_refs.sh` recreates it). Read-only.

- Copy only MIT, BSD, ISC, zlib or Apache-2.0 code, keep its header, and add a row to `THIRD_PARTY_NOTICES.md`.
- yuzu, suyu, OpenSmash-Ultimate and other GPL code: read for behaviour, write our own.
- `tools/vendor/` stays byte-identical to upstream; Windows changes go in `tools/extract.py`.

## Code

- Build C with MSYS2 UCRT64 Clang (`C:\msys64\ucrt64\bin`). Generated C uses GNU extensions, so `cl.exe` is out. Call Ninja from that directory by full path or put it first on PATH.
- Two Python environments: `.venv` (3.12) for the lifter and the build, `.venv-py314` for the extraction and shader scripts, which need `compression.zstd`.
- Generated C is never edited by hand: change the generator (`recomp/`).
- Every lifter change passes the differential tests against Unicorn ([docs/building.md](docs/building.md), "Checks"). After changing `recomp/jumptables.py`, `tests\jumptables_check.py` reports 0 disagreements on 1.2.1 and 1.4.3.
- `recomp/layout.py` alone decides module bases.
- Graphics bugs: [docs/maxwell.md](docs/maxwell.md) lists the Maxwell behaviour the Vulkan path must match and the TotK bugs other ports fixed.
- Comments say what the code does and why, in words that stand on their own. Cite our code by file plus function (a line number may follow as a hint) and upstream code by project, file and function: `refs/` moves, so its line numbers rot. Commit hashes belong to the attribution of copied code.

## Running and debugging

- Running the game, screenshots, frame dumps, timings: [docs/agents/running.md](docs/agents/running.md).
- Investigating a bug, glitch or crash: search [docs/agents/symptoms.md](docs/agents/symptoms.md) by the player's words first, then follow [docs/agents/debugging.md](docs/agents/debugging.md). Read the code before running the game.

## Pull requests

向原作者的 upstream 仓库贡献 PR 时，以其 `dev` 为目标，并遵循 [CONTRIBUTING.md](CONTRIBUTING.md) 的检查要求。本 fork 的分支维护遵循下方约定。代码审查参考 [docs/pr-review-kit/](docs/pr-review-kit/)。

## Writing

Code, comments, docs and commit messages are in English. Commit messages are plain sentences without type prefixes.

## 本 fork 的分支维护

- `main` 是唯一长期维护的集成分支，不再维护本地或 `origin` 上的 `dev`。
- 新需求使用 `feature/<简短英文名称>`，独立 bug 修复使用 `fix/<简短英文名称>`；由代理按任务自行命名。
- 修改代码前确认目标分支和工作区状态；不要把未提交改动带入其他分支。
- 分支合入 `main`，并确认本地和远程分支的提交都已包含在 `main` 后，删除对应的本地与 `origin` 分支。
- 未合并的分支保留；不要为清理分支而擅自合并尚待验证的工作。
- `upstream` 是原作者仓库的参考来源，本约定不要求修改或删除其分支。
