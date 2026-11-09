# M4 Decision Record

See specification Section 8.

# M4 Design: Responsive AI Workspace

## 1. Threads, Ownership, and Shared Data

`MainWindow`, `workspace_`, `processing_core_`, and the widgets belong to the GUI thread. `build_corpus()` copies the workspace and starts a `std::thread`; that worker creates a temporary `ProcessingCore`, waits `--build-delay-ms` per document, and calls `rebuild()`. `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` schedules `build_finished()` on the GUI thread. The main thread joins the worker before reading its result and either accepts or discards its `std::unique_ptr`-owned temporary core. Threads are never detached.

**Shared mutable data (constraint 5):** The GUI writes `build_cancelled_`, and the worker reads it atomically. The worker writes `pending_core_` and `pending_error_`; the GUI accesses them only **after joining** the worker. `workspace_`, generations, current-corpus status, and widgets remain GUI-thread-only. The worker owns its workspace snapshot. The `RemoteAddServer` listener thread uses an atomic `running_` flag and posts document paths to the GUI, never modifying widgets directly. Both worker threads are joined on shutdown.

## 2. Validity and States

States are Not Built, Building, Current, and Failed. At build start, `build_generation_` records `workspace_generation_`. Adding a document or Reset increments the workspace generation and invalidates the corpus. `build_finished()` accepts a successful result **only if the generations match**. Otherwise it discards the old result; failure is reported to the GUI. Search, Inspector, and Context require a current corpus.

## 3. Changes Since the Checkpoint

`DESIGN_NOTE.md` proposed separate `BuildJob` objects and possibly overlapping builds. The final implementation instead manages one `build_thread_`, an atomic cancellation flag, and pending output in `MainWindow`. It prevents another build from starting while one is running. The workspace snapshot, queued completion, generation check, and safe cleanup remain.

## 4. CR-1 Remote Add

CR-1 added `RemoteAddServer`, `start_remote_add_server()`, `remote_add_document()`, `--ipc-socket` in `main.cpp`, and `tools/aiws_send.cpp`. Client syntax is `aiws_send [--socket PATH] add FILE`; relative paths are resolved in the client's working directory. The default socket is `/tmp/aiws-$USER.sock`, or the numeric UID if `USER` is absent. Both processes use POSIX Unix-domain **stream** sockets. A request is **four bytes of unsigned length in network byte order, then exactly that many UTF-8 filename bytes** (no terminator). The server handles partial reads, checks readability, and replies immediately with ASCII `0` (accepted) or `1` (rejected), without waiting for the GUI. Client exit codes are 0, 1, or 2 (success, rejection, or communication failure). A queued GUI call uses `add_document_file()`, preserving M3 behavior and corpus invalidation without changing the backend.

## 5. Tests and Fault Switches

`student_tests` covers background builds, responsiveness, stale builds, Reset, failures, and CR-1 requests including unusual filenames, split writes, rejection, and cleanup. Normal Docker CTest passed. `AIWS_FAULT_ACCEPT_STALE` changes the generation decision in `MainWindow::build_finished()`; stale-build/Reset tests detect it. `AIWS_FAULT_LOSE_FAILURE` suppresses the failed-build completion notification in `build_corpus()`; failure-state tests detect it. Both faulted configurations failed within 60 seconds.

## 6. AI Use

I used ChatGPT to discuss requirements, draft and review parts of the thread/socket/test implementation, troubleshoot Qt and Docker, and edit this document. I tested the program and remain responsible for understanding the submitted code.

## 7. Pair programming log

**Partners:** Vanesha Hari and Raushan Gouredddy

### Session 1: Responsive Corpus Building

**Date:** Sunday, November 8, 2026  
**Start:** 2:30 PM  
**End:** 3:00 PM  
**Codebase:** Vanesha Hari's M4 project  
**Focus:** Background corpus building, generation checks, and delayed-build tests

#### Work Completed

This session focused on moving corpus building off the main Qt thread so the GUI could stay responsive. We looked at the `std::thread` build path, the `--build-delay-ms` option, and the generation counter used to detect when the workspace changes during a build. We also looked at how the worker reports its result back to the main Qt thread and what should happen during Reset or Exit.

#### Driver and Navigator Roles

##### 2:30 PM to 2:45 PM

- **Driver:** Vanesha Hari
- **Navigator:** Raushan Gouredddy

Vanesha worked through the background-build and generation-check logic. Raushan's navigator contribution was identifying the need to handle a document being added before the worker finishes, so an old result cannot become current.

##### 2:45 PM to 3:00 PM

- **Driver:** Raushan Gouredddy
- **Navigator:** Vanesha Hari

Raushan went through delayed-build and Reset testing. Vanesha's navigator contribution was checking the expected interface state, especially that Search, Inspect, and Build Context remain disabled when the workspace has changed.

#### Testing and Challenges

The relevant tests cover normal builds, simulated slow builds, adding documents during a build, Reset during a build, and failure reporting. The tricky part was that moving work to a thread prevents freezing but does not automatically keep the corpus current. Generation checks handle that problem.

### Session 2: Remote Add Document IPC

**Date:** Sunday, November 8, 2026  
**Start:** 6:10 PM  
**End:** 6:40 PM  
**Codebase:** Raushan Gouredddy's M4 project  
**Focus:** Unix-domain socket requests, message framing, and workspace integration

#### Work Completed

This session focused on allowing `aiws_send` to send a document path to the running application. We reviewed how the server reads a four-byte length and the UTF-8 path, handles a request sent in multiple pieces, checks file readability, and passes the accepted path to the GUI. We also looked at how Remote Add should invalidate a build in progress just like the Add Document button.

#### Driver and Navigator Roles

##### 6:10 PM to 6:25 PM

- **Driver:** Raushan Gouredddy
- **Navigator:** Vanesha Hari

Raushan worked through the socket server and client communication. Vanesha's navigator contribution was calling out the need to test partial socket reads rather than assuming that the entire request arrives in one operation.

##### 6:25 PM to 6:40 PM

- **Driver:** Vanesha Hari
- **Navigator:** Raushan Gouredddy

Vanesha went through the workspace integration and expected behavior when a document arrives during a build. Raushan's navigator contribution was reviewing invalid-path handling and whether the server socket is removed on Exit.

#### Testing and Challenges

The Remote Add tests cover valid and invalid document paths, spaces and Unicode in filenames, newlines, split requests, cleanup, and the unavailable-server exit code. During Docker testing, the default socket was `/tmp/aiws-0.sock` because the container ran as UID 0 without `USER` set. Using the correct path allowed the client to connect.

### Reflection

Working through the code together was most helpful when the driver explained the current change and the navigator suggested a concrete edge case to test. On the background-build side, we focused on keeping results consistent after workspace changes. On the socket side, we focused on correctly reading requests and getting them into the Qt GUI without blocking it. Switching roles made it easier to see both the implementation details and the cases that could go wrong.