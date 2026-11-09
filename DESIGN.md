# M4 Decision Record

See specification Section 8.

# M4 Design: Responsive AI Workspace

## 1. Threads, Ownership, and Shared Data

The main Qt thread owns `MainWindow`, the GUI widgets, `workspace_`, and the current `processing_core_`. I kept changes to the workspace and GUI on this thread so that the application would not try to update widgets from a background thread.

When the user clicks Build Corpus, `MainWindow::build_corpus()` takes a copy of the workspace and starts a `std::thread`. The worker builds a separate `ProcessingCore` using that snapshot instead of reading the workspace while it might be changing. The build delay is applied for testing, and the worker calls `rebuild()` without directly updating any widgets. When the work finishes, it uses `QMetaObject::invokeMethod()` with `Qt::QueuedConnection` to notify the main thread. `build_finished()` handles the result on the GUI thread.

There are a few pieces of data shared between the threads. The GUI updates the atomic `build_cancelled_` flag, which the worker can check. The worker prepares `pending_core_` and `pending_error_`; the GUI reads them after joining the completed build thread. The workspace snapshot belongs to the worker while it builds. The generation counters, corpus status, and widgets are handled by the GUI thread. A completed pending core is stored through `std::unique_ptr` until it is either accepted or discarded.

Remote Add has its own `RemoteAddServer` and listener `std::thread`. That thread handles socket requests but does not change the workspace or widgets directly. Instead, it queues the document path to the GUI thread. The main window joins the build thread on shutdown, and the server stops and joins its listener thread.

## 2. Corpus States and Build Validity

I used four main corpus states: **Not Built**, **Building**, **Current**, and **Failed**. Search, Inspect, and Build Context should only be available when the corpus is current for the documents in the workspace.

When a build starts, the application saves the current `workspace_generation_` as `build_generation_`. Adding a document or resetting the workspace increases the workspace generation and makes the previous corpus invalid. When the worker finishes, `build_finished()` checks whether the build generation still matches the current workspace generation. A successful build is only accepted when those values match.

For example, if the user starts building the corpus and then adds another document, the original build is now based on an older workspace. Even if the worker finishes successfully, that result should not replace the current state. Reset works the same way: a build that started before Reset cannot bring back old data after the workspace has been cleared.

Build failures are also handled on the GUI thread so the application does not get stuck showing Building. During Exit, the application handles cancellation and thread cleanup instead of leaving detached workers running.

## 3. Changes From the Design Checkpoint

The initial design checkpoint outlined the need for nonblocking corpus building, workspace generation checks, and safe shutdown. In the final implementation, I added the actual background `std::thread`, the copied workspace snapshot, and queued build-completion handling. I also added `set_build_delay_ms()` and the `--build-delay-ms` argument so I could test the interface with slower builds.

The CR-1 change request added a second thread for the socket server and a separate command-line client named `aiws_send`. It also added the `--ipc-socket` option and the `remote_add_document()` GUI method. Both the Add Document button and Remote Add use `add_document_file()`. That keeps the behavior consistent without changing the existing processing APIs or the main M3 interface.

## 4. Remote Add Document IPC

Remote Add allows an external command-line program to add a file to the running AI Workspace. The client is called using `aiws_send [--socket PATH] add FILE`. The main application accepts an optional `--ipc-socket PATH` argument. By default, it uses `/tmp/aiws-$USER.sock`, or the numeric user ID if `USER` is not set. Relative file paths are resolved from the client's working directory.

The client and server communicate through a POSIX Unix-domain stream socket. A request starts with a four-byte unsigned length in network byte order, followed by that number of UTF-8 bytes containing the path. It does not depend on a newline or a null terminator. Because the server reads the expected number of bytes, it can handle paths containing spaces, Unicode characters, and newlines, as well as messages that arrive in several smaller reads.

The server checks whether the file can be read and sends a one-byte response without waiting for a corpus build. ASCII `0` means the request was accepted, while ASCII `1` means it was rejected. The client uses exit codes `0` for accepted, `1` for rejected, and `2` for a connection or communication error.

After accepting a request, the server queues the file path to the GUI thread. `remote_add_document()` calls `add_document_file()`, which updates the workspace and invalidates a corpus build if one is in progress. When the application exits, the server stops and removes its socket file.

## 5. Testing and Fault Injection

I built the application and ran `student_tests` in the course Docker environment using Qt's offscreen platform. The normal test configuration passed. I also ran the two fault-injection configurations separately and confirmed that the tests failed as expected within the required time limit.

`AIWS_FAULT_ACCEPT_STALE` is located around the generation check in `MainWindow::build_finished()`. It allows an outdated build to be accepted. The stale-build and Reset-during-build tests are meant to catch this because the corpus would incorrectly become current after a workspace change.

`AIWS_FAULT_LOSE_FAILURE` is located where the worker queues its completion notification. It prevents the failed-build notification from reaching the GUI. The failure tests detect this because the interface can remain stuck in the Building state instead of reporting the error.

Other tests cover a normal successful build, GUI responsiveness during a delayed build, adding documents, resetting, remote additions, paths with special characters, requests sent in small pieces, client error codes, and socket cleanup. I also opened the M4 GUI in Linux Qt Creator through WSL to check that the application window launched correctly. The automated tests separately covered the Remote Add behavior.

## 6. AI Tool Use

I used ChatGPT to help understand the requirements, discuss the threading and socket design, draft and review parts of the code and tests, troubleshoot the Linux Qt and Docker setup, and improve the wording of this document. I reviewed the suggestions against the assignment and ran the builds and tests myself. I am responsible for understanding and explaining the code I submit.

## 7. Partner Programming Work

**Project:** ECE 3574, Milestone 4: Responsive AI Workspace  
**Partners:** Vanesha Hari and Raushan Gouredddy

### Session 1: Responsive Corpus Building

**Date:** Friday, November 6, 2026  
**Time:** 3:00 PM to 3:30 PM  
**Codebase:** Vanesha Hari's M4 project

#### Work Completed

For this session, we focused on making the corpus-building process run in the background instead of blocking the GUI. In the previous milestone, corpus building happened on the main GUI thread, so the application could become unresponsive until processing finished.

We went through the implementation using `std::thread` and reviewed how the `--build-delay-ms` option could simulate a slower build. We also looked at how generation counters track changes to the workspace. If a document is added or the workspace is reset while a build is running, the generation changes so the old result will not be accepted.

We also reviewed how build results and errors get passed back to the main Qt thread, along with how the worker thread is cleaned up during Exit.

#### Driver and Navigator Roles

**3:00 PM to 3:15 PM**  
Driver: Vanesha Hari  
Navigator: Raushan Gouredddy

Vanesha reviewed the background-building and generation-check code while Raushan focused on cases where the workspace changes during a build. A useful point to check was what should happen if another document is added before the worker finishes.

**3:15 PM to 3:30 PM**  
Driver: Raushan Gouredddy  
Navigator: Vanesha Hari

Raushan reviewed the delayed-build and Reset testing cases while Vanesha checked the expected GUI behavior, including whether Search, Inspect, and Build Context stayed disabled when the corpus was no longer current.

#### Testing and Verification

The test cases for this part covered normal builds, delayed builds, adding a document during processing, and resetting during a build. The automated fault configurations also checked whether the tests could catch an outdated result being accepted or a failed build not being reported.

#### Technical Challenges

The main challenge was making sure the GUI stayed responsive without using outdated data. Moving the build to another thread fixed the blocking problem, but the workspace could still change before the worker finished. Generation checks were important for keeping the result consistent with the current documents.

### Session 2: Remote Add Document IPC

**Date:** Saturday, November 7, 2026  
**Time:** 3:00 PM to 3:30 PM  
**Codebase:** Raushan Gouredddy's M4 project

#### Work Completed

For this session, we focused on Remote Add Document. The goal was to let the `aiws_send` command-line program send a document path to the running application without using the Add Document button.

We reviewed the Unix-domain socket connection between the client and the server. The request uses a four-byte length followed by the UTF-8 document path, which allows the program to handle filenames with spaces, Unicode characters, and newlines. The socket server reads the complete request, checks the file, and queues the addition to the GUI thread.

We also looked at how Remote Add uses the same document-addition logic as the normal GUI. This matters when a remote document arrives during an active corpus build because that build should become outdated.

#### Driver and Navigator Roles

**3:00 PM to 3:15 PM**  
Driver: Raushan Gouredddy  
Navigator: Vanesha Hari

Raushan reviewed the server and `aiws_send` implementation while Vanesha focused on the message format. One important edge case was testing a request that arrived in small pieces rather than in a single read.

**3:15 PM to 3:30 PM**  
Driver: Vanesha Hari  
Navigator: Raushan Gouredddy

Vanesha reviewed the workspace integration and Remote Add tests while Raushan looked at the socket behavior, including invalid paths and cleanup when the application exits.

#### Testing and Verification

Remote Add testing covered adding a document through `aiws_send`, adding a document while the corpus was building, handling special filenames, reconstructing a request sent one byte at a time, removing the socket file on Exit, and returning exit code `2` when the server was unavailable.

#### Technical Challenges

One issue during Docker testing was identifying the correct socket path. The container ran as UID 0 without a `USER` environment variable, so the default socket path was `/tmp/aiws-0.sock`. Using that path allowed the socket communication test to connect.

We also had to make sure Remote Add would not bypass the workspace validity checks. Sending the path to the same `add_document_file()` function kept remote additions consistent with manual additions.

### Overall Pair Programming Reflection

The two sessions covered different parts of M4 that needed to work together. The first focused on background threads and preventing outdated corpus results. The second focused on socket communication and getting remote document requests safely into the GUI.

The most useful kind of interaction was discussing expected behavior and edge cases while one person reviewed or tested the code. Being the navigator made it easier to think about things that could go wrong, like Reset during a build or a socket request arriving in multiple pieces. Switching roles also gave both of us a chance to explain the implementation and review the other person's work.

The main thing we learned was that making the GUI responsive is not enough by itself. The application also has to keep workspace state consistent and handle incoming requests and shutdown correctly.
