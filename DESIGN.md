# M5 Design for AI Workspace

**M4 fixes.** I added `test_repeated_builds()` to check that I could build once, add a second document, and build again in the same window. I also fixed how the remote client handles relative paths. The server can read a request that arrives in pieces, including filenames with spaces, Unicode, or newlines.

**Threads and shared data.** The GUI thread owns the workspace, widgets, active corpus, and state objects. The first call to `build_corpus()` starts a worker that handles every build until Exit. Between builds, `wait_for_build()` waits on `build_ready_`.

I used `build_mutex_` to protect the two things the GUI and worker both need to access: `pending_build_` holds the next request, and `stop_worker_` tells the worker when to exit. Each request carries its own workspace copy, generation number, and delay. The worker reads the atomic `workspace_generation_` to check whether its copy is still current. It sends the finished core back through a queued GUI callback. The remote listener has its own atomic `running_` flag and sends accepted paths to the GUI. Its socket stays open until the listener joins.

**Corpus states.** The workspace starts in `NotBuilt`. Once I add a document and click Build, it enters `Building` and ignores another Build click while the worker is busy. If the build succeeds, the state becomes `Current` and I can use Search, Inspect, and Context. If the build fails, it enters `Failed` and I can click Build to retry. I can also rebuild a current corpus. Adding a document or pressing Reset returns the workspace to `NotBuilt` from any state.

**Build and Reset.** I chose to ignore a second Build click while a build is running. `Building::request_build()` handles that decision. A remote add follows the same path as a local add through `add_document_file()`. If it interrupts a build, the GUI shows “Workspace changed. Rebuild required.”

1. When I press Reset during a build, `reset_application()` clears the workspace. On the GUI thread, `invalidate_corpus()` advances the generation and clears any pending request.
2. On the worker thread, `WorkerChunker::check_generation()` detects the change at a document boundary. The worker returns to `worker_loop()` for the next request.
3. If the worker already sent the old result, `build_finished()` checks its generation on the GUI thread and discards it. On Exit, the destructor sets the stop flag, wakes the worker, and joins it.

**Testing.** With `AIWS_FAULT_NO_CANCEL`, the old build keeps running. `test_replacement_build()` catches the delay. With `AIWS_FAULT_SILENT_SHUTDOWN`, the wait condition in `wait_for_build()` ignores the stop flag. `test_idle_shutdown()` catches the hang. The normal tests passed, and each fault triggered its expected failure within six seconds. ThreadSanitizer stopped at startup with an unexpected memory mapping, so I couldn't verify whether it detected any races.

**AI use.** I used ChatGPT to review the requirements, draft and revise code and tests, troubleshoot Docker output, and help write this design record.
