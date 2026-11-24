# M5 Design for AI Workspace

**1. M4 fixes.** I added `test_repeated_builds()` to check that I could build, add a second document, and build again in the same window. I also fixed how the remote client handles relative paths. The server now reads a request that arrives in pieces, including filenames with spaces, Unicode, or newlines. A multi-word term in the Term Inspector crashed the app; `term_inspection_requested()` now reports it. Reset now shows "Ready." even during a build.

**2. Threads and shared data.** The GUI thread owns the workspace, widgets, `processing_core_`, and `lifecycle_`. The first `build_corpus()` starts the build worker, which runs `worker_loop()` until Exit. The CR-1 listener runs `RemoteAddServer::serve()`.

- `pending_build_` and `stop_worker_` are guarded by `build_mutex_`. The GUI writes them in `build_corpus()`, `invalidate_corpus()`, and `~MainWindow()`. The worker reads and clears them in `wait_for_build()`. Every access holds the lock, and the worker sleeps on `build_ready_` between builds.
- `workspace_generation_` is an atomic counter. The GUI increments it, and the worker reads it in `check_generation()`.
- `RemoteAddServer::running_` is an atomic shared by the GUI and the listener. Accepted paths reach the GUI through a queued call.

Each request carries its own workspace copy, so the worker never touches `workspace_`. The finished core returns through a queued call to `build_finished()`.

**3. States** (`CorpusLifecycle`):

| State | Meaning | Build | Add / Reset | Build finishes |
|---|---|---|---|---|
| `NotBuilt` | no corpus for this workspace | → `Building` | stays | ignored |
| `Building` | worker is building | ignored | → `NotBuilt`, build cancelled | → `Current` or `Failed` |
| `Current` | Search, Inspect, Context enabled | → `Building` | → `NotBuilt` | ignored |
| `Failed` | last build failed | → `Building` | → `NotBuilt` | ignored |

Exit works the same in every state: the destructor stops and joins the worker.

**4. Trace: Reset during a build.**
1. GUI: `reset_application()` clears the workspace and calls `invalidate_corpus()`.
2. GUI: `Building::invalidate()` moves to `NotBuilt`, `workspace_generation_` advances, and `pending_build_` is cleared.
3. Worker: `WorkerChunker::check_generation()` sees the new generation at the next document boundary and throws `BuildCancelled`.
4. Worker: `worker_loop()` catches it and waits in `wait_for_build()` for the next request.
5. GUI: if an old result was already queued, `build_finished()` sees the stale generation and discards it.

**5. Decisions.**
1. *Build during a build:* `Building::request_build()` ignores it, and `update_action_states()` disables Build Corpus while building.
2. *Remote add mid-build:* `remote_add_document()` calls `add_document_file()`, which calls `invalidate_corpus()`, so the build is cancelled exactly like a local add. The status shows "Workspace changed. Rebuild required."
3. *Shared data:* the worker needs a request to take, a stop signal, and a way to tell during a build that its copy is outdated. Without the generation it could not cancel mid-build, and without the request and stop flag it could not wait on `build_ready_`.

**6. Fault switches.** `AIWS_FAULT_NO_CANCEL` is in `WorkerChunker::check_generation()` in `main_window.cpp`: it never throws, so the old build runs to the end. `test_replacement_build()` and `test_busy_shutdown()` catch it. `AIWS_FAULT_SILENT_SHUTDOWN` is in the wait predicate in `wait_for_build()`: it ignores `stop_worker_`, so Exit hangs while the worker is idle. `test_idle_shutdown()` catches it. The normal tests pass, `NO_CANCEL` fails in about 12 s, and `SILENT_SHUTDOWN` fails in about 5 s.

**7. AI use.** I used ChatGPT to review the requirements, review code and tests, troubleshoot Docker output, and help draft this record. 
