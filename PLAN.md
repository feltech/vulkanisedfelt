<!--
vulkanisedfelt
SPDX-License-Identifier: MIT
Copyright 2026 David Feltell
-->

# Monadic Vulkan migration

## Direction and scope

Replace the imperative demo with a program composed from very small, named functions using fnfelt.
Wrap atomic Vulkan operations in deferred IOs, compose dependent work explicitly, and use `sequence`
and `ap` for independent work where concurrency is safe. ReaderIO supplies long-lived capabilities
and configuration; StateIO threads logical application state. Layer these pieces into the existing
demo while keeping them reusable for other demos.

The goal is both runtime structure and readable compile-time diagnostics. Named monads and
reflection-backed validation should identify the failing application stage instead of exposing a
wall of nested template and lambda types. Small named functors and functions remain important even
with library diagnostics.

Start from this branch's imperative implementation. The abandoned `monads` branch in this repository
is an architectural reference and source of useful policy, not a framework to restore or merge
wholesale. Develop fnfelt further when concrete migration work demonstrates a generic need. Do not
replace it with another project-local monad framework.

The first parity target is the actual demo in `src/vulkandemo.cpp`: create a window and Vulkan
resources, clear/present frames, process quit/resize, recreate swapchain resources, and clean up. It
currently does not create a shader/graphics pipeline or issue geometry draw calls. Pipeline-layout
and vertex-buffer helpers exist separately; inventory them, but do not turn a triangle renderer into
an unstated prerequisite for migrating the clear-window demo. Additional demos can build on the
generic components later.

## Completed foundation

The setup work is complete. An isolated default-instance slice now exists under `src/vulkan/`, with
real and mocked APIs under `src/backend/`, reflected mock conformance, scoped managed handles and a
lazy ReaderIO creation action. Global instance extension and layer enumeration return Immer arrays
filled directly through transients, with three bounded count/fill attempts and VK_INCOMPLETE on
exhaustion. Mocked tests and real headless instance and enumeration smoke tests pass. The existing
renderer remains intact; the broader inventory, execution contracts and ordered migration phases are
still pending.

- fnfelt is a Git submodule at `vendor/fnfelt`, linked through `fnfelt::fnfelt`. The current pin is
  `b421fd36b53e7b1db3183347ecd7524871b6c3ff`, including `io::on` and the standalone main-thread
  dispatch demo. The library tests establish context entry/restoration for values, Result outcomes,
  exceptions and move-only values. The demo establishes the successful dispatch path, not complete
  renderer shutdown or resource cleanup.
- Git submodule consumption is the selected approach. Retain it for this work. For joint library
  development, commit changes in fnfelt and then update the parent gitlink deliberately. Do not
  silently switch the build to an unrelated local checkout or a moving dependency revision.
- `conanfile.py` supplies Immer 0.9.1, libfork 3.8.0 and the existing demo dependencies, including
  doctest 2.5.3. fnfelt resolves its pinned stdexec dependency. Its development suite stays disabled
  in this consumer build.
- Nix uses GCC/libstdc++ 16.2 with reflection support and the fnfelt development tools. CMake
  requires at least 3.30. Root CMake sets C++26 before fnfelt's reflection capability probe, because
  the probe runs before target interface requirements can supply the language mode. Keep this
  workaround until a separately tested library fix makes it unnecessary.
- Root `CMakeLists.txt` owns options/dependencies; `src/CMakeLists.txt` owns the `vulkandemo`
  executable. Keep related private headers/implementations under `src/`; introduce reusable public
  headers under `include/vulkanisedfelt/` and mirror their hierarchy under `src/` as needed.
- The only project switches are `vulkanisedfelt_ENABLE_DEV` and
  `vulkanisedfelt_ENABLE_SANITIZER_ASAN`. Legacy `vulkandemo_` CMake switches were removed at the
  user's request. DEV enables embedded doctests and strict linting.
- Formatting, cpplint, CMake lint/format, Doxygen and Markdown checks gate DEV builds. Markdown
  checks include README and recurse through `.agents/`. clang-tidy is disabled until it supports
  reflection; apply its naming rules manually. Existing sources have been brought into the lint
  baseline without intentionally changing demo behavior.
- SDL's ALSA/PulseAudio backends default off through overridable Conan options. The demo initializes
  only SDL video. The original defaults pulled in audio servers/codecs and caused a very expensive
  GCC 16 rebuild, including several minutes in PulseAudio's libtool link script.
- `AGENTS.md`, `.codex`, `.agents/skills` and formatter/linter configuration follow fnfelt's
  conventions, adapted for this executable project. Read current guidance before implementation; it
  remains authoritative and may have user edits newer than this plan.

The setup history has been reviewed/autosquashed by the user. The earlier imperative baseline was
`84c6989`. The observed `monads` ref is `b60c7e3f940e7e05e1cc393a8a4d55fa92fc03f7`; do not assume
older prototype hashes or line numbers still identify its contents. Always inspect status, current
refs and submodule state before work, and preserve unrelated edits in both repositories.

### Build and verified coverage

Outside the Nix shell, prefix development commands with `nix develop ./build_env --command `, or use
the project's direnv environment.

```bash
git submodule update --init --recursive
conan install . --build=missing -s "&:build_type=Debug" \
  -pr:a build_env/nixos.linux.conan.profile
cmake --preset conan-debug -Dvulkanisedfelt_ENABLE_DEV=ON
cmake --build --parallel --preset conan-debug
ctest --test-dir build/Debug --output-on-failure --parallel --timeout 300
```

Conan's Python recipe uses `cmake_layout`: do not copy an `-of build/Debug` command from a
layout-free recipe, which can create an extra nested build directory. Its generated preset points at
`build/Debug/generators/conan_toolchain.cmake`. If making a separate disposable build directory,
pass that toolchain by absolute path; simply overriding the preset's binary directory leaves its
relative toolchain path pointing at the wrong location. Use compiler tools from PATH.

Verified during setup:

- Dependency install, fresh reflection-capable configure, complete DEV build and all lint gates.
- Both current CTest targets: `vulkandemo` (embedded real Vulkan/SDL tests) and
  `vulkanisedfelt.fnfelt` (CPU-only dependency consumer). Both passed on this machine.
- A non-DEV build with `DOCTEST_CONFIG_DISABLE` compiled successfully.
- `tests/fnfelt.cpp` exercises IO `sequence`, ReaderIO/StateIO lifting, `store`, Immer and libfork
  execution, and includes the stdexec bridge header. It proves basic consumer integration; it does
  not execute a sender, exercise platform scheduling, or validate Vulkan ownership/concurrency.

Run the current CPU check with `ctest --test-dir build/Debug -L cpu`. Existing demo tests use a real
driver/display; their success does not make the suite driverless. The completed migration needs
separate pure/fake and real-driver test tiers.

## Starting code and prototype lessons

Use `git show monads:<path>` and `git ls-tree -r monads` to inspect the prototype without replacing
the current checkout.

| Area            | Starting point and lesson                                                                                                                                                                                                                                                           |
| --------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Entry point     | Current `src/vulkandemo.cpp` is imperative. The prototype's same file composes some setup monadically, then repeats setup imperatively and keeps an imperative frame loop. Port one complete path rather than perpetuating both.                                                    |
| Wrappers        | Prototype `src/setup/monadic.hpp` contains a large adapter inventory. Use it to discover responsibilities, but do not port the entire file before delivering a tested slice.                                                                                                        |
| Generic helpers | Prototype `src/monad/`, higher-order helpers and macros predate fnfelt. Map their useful behavior to existing fnfelt operations; do not copy their implementation by default.                                                                                                       |
| Selection       | Prototype `src/setup/filters.cpp` contains useful queue/memory policy, but some filters still query the driver for logging. Extract truly pure selection over snapshots. Audit returned C-string views and backing storage; selection results should own names and capability data. |
| Ownership       | Current/prototype `src/types.cpp` retain parent owners in shared-pointer deleters. This preserves some lifetimes, but hides Vulkan calls in destruction. Preserve dependency lifetimes while making teardown an observable effect.                                                  |
| Outcomes        | Inspect `src/draw.cpp` and the prototype equivalent: suboptimal acquisition and ignored presentation status need command-specific handling. Do not carry forward a blanket non-success-means-throw macro as the outcome model.                                                      |
| Synchronization | Current code waits for the queue each frame and the device during resize. Begin conservatively; prove correctness before introducing overlap or replacing these waits with more precise synchronization.                                                                            |
| Tests           | Existing setup/draw tests create real Vulkan and SDL resources. Retain useful coverage while introducing independently runnable CPU-only policy and fake-dispatch tests.                                                                                                            |

The current resource path is window -> layer/extension negotiation -> instance -> optional debug
messenger -> surface -> physical device/graphics-present queue selection -> device/queues ->
semaphores -> swapchain/image views -> render pass/framebuffers -> command pool/buffers. The loop
polls events, changes clear colour on resize, rebuilds swapchain/framebuffers, acquires an image,
records a clear render pass, submits, presents, and waits for the queue. Inventory actual calls,
helper coverage and shutdown requirements before deciding the final module boundaries.

## Architecture contracts

These are migration design contracts and acceptance criteria. They are not claims that the current
demo or the basic consumer test already implements them.

### Pure policy, named actions, composition

Use three layers:

1. Pure owned data and decisions: extension/layer requirements, capability snapshots, device
   scoring, queue selection, swapchain format/extent/image count, render descriptions and lifecycle
   decisions. These should be ordinary, preferably constexpr functions with no live driver queries.
2. Small named effect actions: Vulkan calls, dispatch loading, platform operations and logging.
   Constructing or discarding an unexecuted plan must make no loader/driver/platform calls. An
   atomic wrapper normally represents one operation; enumeration and acquisition rollback need
   clearly specified multi-call protocols around those operations.
3. Application orchestration: ReaderIO supplies capabilities, StateIO threads logical transitions,
   and a small application execution boundary runs the program while main services platform work.

Maintain a command inventory covering loading, global/instance/device queries, creation, recording,
synchronization, presentation and destruction. Every actual Vulkan invocation belongs inside the
effect boundary, including void recording functions and calls hidden in deleters or convenience
helpers. Port the commands used by the demo first, not the whole Vulkan API.

Try a project-local API object borrowed through ReaderIO: `backend::Api` forwards to real Vulkan and
platform functions, while `backend::MockApi` supplies per-test expectations and scripted results.
The environment/API type selects the implementation at compile time. Keep action factories generic
over that type and use deduction through composition, rather than requiring explicitly supplied API
template arguments throughout the renderer. This is the selected design to try, not a proven
implementation.

Keep the seam command-shaped, preserving Vulkan results and output parameters so tests exercise the
actions' result handling, enumeration and ownership logic. Real wrappers should qualify C API calls
to avoid member-name hiding and retain runtime-loaded global/instance/device dispatch with the
appropriate scope. Pure and mocked impure tests may link Vulkan and SDL; retaining build/link
coverage is useful even when real-API tests are filtered out. Try trompeloeil member mocks for
expectation support. Fakes record calls, arguments, parent relationships and scripted outcomes; they
test adapter contracts rather than implementing Vulkan. Use portable fake resource representations,
not assumptions that all handles are integers or that arbitrary pointer casts are safe.

After extension negotiation, validate dispatch pointers. Missing optional entry points disable the
feature under explicit policy; missing required entry points return a named runtime error. Cover
debug messenger entry points in fake tests. A runtime null pointer is distinct from a missing
capability type caught at compile time.

### ReaderIO and StateIO semantics

Prefix action factory functions with the returned monad: `io_`, `reader_io_`, or `state_io_`. For
example, `reader_io_vk_create_instance` constructs a ReaderIO. Simple stored actions use free
functions and `std::bind_front`; named action structs retain an `IOAction` or `ReaderIOAction`
suffix when needed. Pure calculations keep ordinary operation names. Group pure selection policy and
deferred discovery in the same feature header, with header-visible implementation helpers under
`detail/`.

Reader environments should be small, copyable immutable views of dispatch, session capabilities,
configuration and the handles relevant to a stage. The active run owns their referents. Avoid a copy
of the mutable renderer or a mutable global state bag disguised as a Reader environment.

Borrow the API object through a member such as `std::reference_wrapper<TApi> api` in a generic
environment. Trompeloeil mocks are non-copyable; do not embed them by value in the copied
environment. All environment copies must reach the same API object and expectation state. The
session or test owns that object and keeps it alive through all outstanding work and cleanup. Do not
route mocks through global or thread-local current-API state. An immutable Reader view can provide
effectful operations; the real wrapper may itself be stateless, while loaded dispatch and mock
expectations live in their respective implementations. Keep this capability in ReaderIO and logical
application facts in StateIO.

State snapshots should represent logical facts: selected device/queue identities, resource
generation, drawable extent, frame slot and recreate/shutdown decisions. Distinguish owners,
borrowed handles and plain descriptions. Copying state must not duplicate resource ownership or
invoke the driver. `immer::box` is a candidate for cheap immutable snapshots, not a requirement for
every value. Never persist pointers into temporary vectors, strings, create-info storage or `pNext`
chains. Reject session/generation-bound actions after their referents expire.

Respect and recheck the pinned library's actual contracts:

- ReaderIO `build` copies an lvalue environment or moves an rvalue; ReaderIO chaining can copy the
  environment between stages. Relevant paths must support that copying.
- State `get_state` and Reader-to-State lifting require copyable state. Direct Reader-to-State
  lifting uses a matching state/environment type; do not assume separate unrelated types compose
  through that overload or infer blanket move-only support from another lifting operation.
- `store` runs its mutator only for a successful `Result`; errors and stops retain the state already
  produced by the source. A state update cannot undo an already executed Vulkan operation.
- IO/ReaderIO `sequence` and applicative composition may overlap independent work. StateIO
  `sequence` threads state serially. Never treat the spelling `sequence` as a serial execution
  guarantee across all monads.
- `tap` is not a finally/cleanup facility and discards the source value. `select` and `repeat_until`
  have their own fallback/accumulator semantics; do not treat them as general error transformers.

Use `transform` for pure results, `and_then` for dependent effects, and explicit lifts between
layers. For a `Result` source, both combinators receive only successful payloads and propagate
errors and stops. Use `and_then` when the next action can fail; returning a `Result` from a
`transform` of an existing `Result` nests that outcome as successful data. Parallelize only where
both resource dependencies and Vulkan host-synchronization rules permit it. Begin with serial access
to queues, command pools and other externally synchronized objects. CPU task completion is not GPU
completion.

### Outcomes, ownership and cleanup

Use one error type per compositional domain. Fallible Vulkan actions return `vulkan_result<T>`, a
`fnfelt::Result<T, VulkanFailure>` whose failure identifies the `FallibleVulkanOperation` and
original `VkResult`. Each atomic action supplies its operation at the failure source, so low-level
compositions share an error type without conversion. Renderer workflows return `renderer_result<T>`
with a shared `renderer_error` variant containing contextual `VulkanOperationFailure` and policy
failures such as `NoSuitablePhysicalDevice`. Extend that domain as renderer workflows grow; keep
unrelated subsystems' errors separate. Recoverable frame statuses need their own payload-bearing
representation. `Result` distinguishes success, error and stopped states. Use the `fnfelt::result`
factories, predicates and accessors rather than the underlying optional/expected representation: an
engaged outer optional can contain an error. Pure selectors may still return `std::optional` for an
ordinary absence of a suitable candidate.

Use `transform_error` at meaningful subsystem boundaries to translate errors and add context. Device
probes retain `VulkanFailure` until renderer orchestration attaches the original device index;
instance-wide failures have no device index. Avoid a separate error type per function or an
application-wide variant shared by every atomic action. `and_then` then bypasses later acquisitions
on errors and stops. Recover deliberately with `or_else`, which runs only for errors and uses the
same Reader environment; successful and stopped outcomes bypass recovery. Keep recovery policy at
the caller that can choose a retry, relaxed requirement or shutdown. Missing requirements remain
explicit domain errors; an unsuitable candidate does not stop discovery of the remaining devices.
Reserve stopped outcomes for an explicit cooperative skip/cancellation decision, and do not turn
every Vulkan failure into a stop.

`traverse` and `sequence` collect child outcomes rather than short-circuiting each child's `Result`.
Independent probes must finish before their ordered outcomes are inspected. Headless selection
returns the first non-success in enumeration order, preserving either its stop or its indexed query
error. Test propagation, recovery failure, success/stop bypass, ordering and retained owners.

Runtime branches must return one compatible C++ monad type. An ordinary deduced-return `if` cannot
return unrelated `io::just(error)` and `create_instance(plan)` instantiations. A small stored action
with an explicit common `fnfelt::Result<...>` return type is a valid initial solution. Prefer
Result-aware composition where it removes such branching. Keep actions small; do not hide the whole
renderer in a single effect to avoid type composition.

Enumeration needs count/fill handling, empty results, changing counts, `VK_INCOMPLETE`, capacity
checks and a documented bounded retry/failure policy. Build temporary C-string/argument views from
owned plans inside the action that consumes them. Preserve the acquired image index on
`VK_SUBOPTIMAL_KHR`; out-of-date acquisition supplies no image to render. Inspect presentation
results, timeout/not-ready and device loss according to each command's contract.

Choose ownership with each concrete resource slice rather than requiring a generic session or
cleanup registry. Acquire and destroy resources on their required context. If owner allocation or
publication throws after acquisition, action-local rollback destroys the resource before failure
escapes. Reader/State values may borrow views only while the concrete owner remains alive.

Normal completion, returned errors, exceptions and cooperative stop must share explicit cleanup:

1. Stop starting new work and join outstanding CPU/sender operations.
2. Establish required GPU completion while parents and dispatch remain valid.
3. Destroy resources in reverse dependency order: swapchain/device children before their parents,
   surface before its instance/window, debug messenger before instance, and platform resources on
   their required thread.
4. Release dispatch/loader/executor services only after their users finish.

No last shared-pointer release in pure state or discarded plans may silently call Vulkan. The first
instance slice experiments with scoped shared ownership in `src/vulkan/managed_handles.hpp`:
creation and final release occur within an active scope, destruction uses the borrowed API, and the
API must outlive every owner. This does not establish asynchronous cleanup or GPU completion. Beyond
this bounded experiment, emergency RAII remains a non-throwing fallback within the active effect
scope, routed through observable dispatch. Async cleanup must run through an explicit coroutine
path; destructors cannot `co_await`. Preserve the original failure when cleanup also fails and
report cleanup errors separately. Preserve allocation-callback compatibility and parent lifetimes.

Reusable factory plans acquire fresh resources on every run. Generation-bound draw actions are valid
only for their live generation. If an action consumes state and is single-use, document and reject
reuse deliberately. Test discarded plans, repeated runs, stale generation use and every partial
acquisition/owner-publication failure, including allocation exceptions and late failures.

### Platform threading and execution topology

The selected topology follows fnfelt's `tests/demo/main_thread_dispatch.cpp`: an ordinary background
thread calls `sync_wait` on a libfork pool while the main OS thread services a libfork worker
context. Do not add another generic project-local runner, session or outcome-report framework.
Integrate this small boundary with the migrated SDL path when that path needs it; keep the legacy
entry point unchanged until replacement is ready.

The previously inspected libfork 3.8 mechanism is:

- Initialize the context on the main thread with
  `lf::worker_init(lf::nullary_function_t{notification})` from `<libfork/core.hpp>`.
- On that same thread, drain `main_context->try_pop_all()` and pass returned jobs to `lf::resume`.
  Drain outside an already running libfork coroutine; resumption requires an empty worker stack.
- Wrap each actual short platform operation in `fnfelt::monad::io::on(main_context, operation)`.
  ReaderIO can obtain the context from its environment and return that IO. `io::on` executes the
  source on the target and restores the originating context before returning its value, Result
  outcome or exception. Child suspension/fork/join can still migrate; wrapping a whole renderer
  subtree does not establish permanent affinity. Cleanup needs its own placement.
- The notification runs inline on the submitting thread and may be invoked concurrently. It must be
  thread-safe and non-throwing; wake the main loop there, rather than touching SDL objects or
  resuming libfork jobs. A no-op requires bounded polling, not an indefinitely blocking event wait.
- Use an atomic `should_main_thread_awake` doorbell: notification exchanges true with release
  ordering and calls `notify_one`; main exchanges false with acquire ordering before checking the
  queue, then uses `wait(false, relaxed)` when no completion is pending. Clear before checking so a
  racing submission cannot lose its notification. The next acquiring exchange observes submitted
  work publication after a relaxed wait.
- The initialized main thread is a libfork worker. Calling `lf::schedule`/`lf::sync_wait` there can
  throw `schedule_in_worker`. The launcher must be an ordinary thread, not another initialized
  libfork worker. `sync_wait` does not itself pin its program to the calling thread.
- Main-loop completion is a runtime concern, outside StateIO/ReaderIO state. After the background
  `sync_wait` and all cleanup needing dispatch finish, set a separate atomic completion flag with
  relaxed ordering and ring the same doorbell. Main joins the background thread before reading its
  result, releasing main-owned SDL resources and finalizing its context, in that order. The join
  supplies result synchronization. Never stop dispatch or block main waiting for work that still
  needs its queue.

The demo's final `and_then` is success-only: Result errors and stopped outcomes bypass it. The
application boundary must signal completion regardless of the returned Result state. Do this after
the ordinary background wait rather than adding a terminal IO solely to stop main. Unexpected
exceptions require either deliberate fail-fast behavior or a small completion guard before future
propagation; they do not justify a new outcome framework.

Application cancellation is distinct from runtime completion. SDL quit should request cooperative
stop through a token available to the program, checked at frame boundaries and before/after long
work. It must not stop main dispatch while outstanding operations or cleanup still need it.

Publish owned SDL event/resize data through a synchronized mailbox. An atomic quit flag does not
make the rest of the renderer thread-safe. Only the main thread accesses platform-alive state;
before SDL initialization and after teardown, do not poll SDL. Use a bounded non-SDL wait/wakeup
path then. Publish the root outcome safely and read it after completion/acquire or join. Include
launcher-construction failure, main-loop exceptions, concurrent notifications and shutdown wakeups
in the execution spike. The pool, dispatch, callbacks and services must outlive all submitted work.

Async leaves created with fnfelt's `create_async` use stateless coroutines and explicit arguments;
do not capture window/context state in a coroutine lambda. Main-thread sender scheduling and SDL3
integration are optional later work, not prerequisites for this SDL2 migration.

### Existing stdexec interoperability

fnfelt already has `<fnfelt/interop/stdexec.hpp>` and `await_sender`; do not plan to implement the
sender-to-libfork bridge again. Its TCP echo demo under `vendor/fnfelt/tests/demo/` is useful
service-lifetime evidence. Use sender operations where they add value, not as another wrapper around
every synchronous Vulkan call.

Inside a libfork task, immediately await with the current `self.context()`; do not cache that
context across suspension or a main-thread hop. The bridge outcome is
`fnfelt::interop::sender_outcome_t<Sender, Environment>`, a `Result<Value, Error>` with
sender-provided error payloads, using a variant when several distinct alternatives are advertised
and Infallible when none are advertised. Map sender errors into application outcomes with
`transform_error`. Returning the Result from an action enables the usual error/stop propagation
through its continuations; awaiting it inside a coroutine does not itself request shutdown or run
cleanup. Stop tokens affect their sender only. Completion is noexcept: storing owned completion
values or errors must not throw, otherwise execution terminates. Connection and result-move
exceptions propagate normally. Keep external services, callback state and borrowed payload referents
alive through completion.

The bridge does not supply SDL affinity or a main-thread scheduler. A blocking stdexec run loop
cannot simply be inserted into the SDL polling loop. Prefer the explicit libfork context hop above
for the initial integration.

### Diagnostics and library evolution

Name public wrappers and composition boundaries: setup, selection, state transitions, rendering and
cleanup. fnfelt names are compile-time static-storage `char const *` template arguments,
conveniently written with `_ss`, for example `io::create<"create_instance"_ss>(action)`. Names are
optional in the library; make them an application convention. Prefer supported factories/combinators
over direct construction that can bypass their validation.

Add direct validator assertions for individual invalid conditions and a small downstream
compile-fail suite proving the nearest useful named boundary for wrong callbacks/results,
incompatible state, missing capability types and accidental non-monad returns. Arbitrary syntax,
name lookup and errors inside callback bodies cannot all become friendly static assertions.

The following are candidates, not existing APIs or blanket requirements:

| Candidate                    | Trigger and required contract                                                                                                                                                                                                                                                                        |
| ---------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Scoped acquire/use/release   | Correct cleanup is required immediately; a generic library primitive is conditional on proven reusable semantics. Acquire/release once per run, handle partial acquisition and exceptions, join children, preserve original failures and support repeat execution without construction-time effects. |
| Explicit serial composition  | Add only if existing dependent chaining is insufficient. Preserve Reader environment and distinguish ordering from Result propagation; never change existing parallel `sequence` semantics.                                                                                                          |
| Further dispatch convenience | `io::on` already supplies context entry/return. ReaderIO/StateIO overloads or `and_then_on` are optional only when concrete composition demonstrates a need.                                                                                                                                         |
| Validation/name propagation  | Fix when representative migrated failures bypass useful diagnostics. Validate before instantiating dependent types and preserve the nearest named stage.                                                                                                                                             |

Keep additions Vulkan-independent and narrowly justified. Follow fnfelt's own `AGENTS.md`, its
reflection/Immer skills and injected-validator pattern; list new public headers in its FILE_SET. Run
its development tests, linters and relevant compile-fail cases separately: the parent consumer does
not run fnfelt's suite. Commit library changes separately, then advance the submodule pin with the
corresponding consumer change.

## Ordered migration and acceptance gates

Each step should deliver a reviewable slice, not a giant adapter inventory followed by a final
untested composition. Proposed module names are responsibility areas, not a fixed directory design.

1. **Inventory and execution contracts.** Record every Vulkan/SDL command used by the current demo,
   its prototype counterpart, ownership/ordering requirements, intended module and test tier.
   Inventory assets and unused pipeline/vertex helpers without assuming shaders exist in the demo.
   Prove environment/state copying, common-type error branching, repeated execution and partial
   acquisition cleanup. Reuse fnfelt's dispatch demo and `io::on` tests, then prove application
   shutdown for successful, error and stopped results when the SDL path is integrated. Acceptance:
   runnable focused tests and recorded ownership/executor contracts before moving window or resource
   ownership. No broad framework changes yet.
2. **Pure types, outcomes and dispatch seam.** Introduce owning capability snapshots, selection
   plans, command-specific outcomes, the borrowed API environment and mock implementation. Retain
   useful prototype policy while eliminating borrowed C-string lifetimes. Acceptance: pure/fake
   tests make no real API calls and need no GPU or display; environment copies share one mock and
   its expectations; constructing/discarding plans records zero calls; required and optional missing
   dispatch paths are tested; named diagnostics are checked at a small boundary.
3. **First complete vertical slice, SDL-free.** Enumerate layers/extensions, select requirements,
   create an instance and optional debug messenger, and tear down deterministically. ReaderIO
   supplies the borrowed API/configuration and StateIO records successful transitions. Use this
   slice to evaluate the real/mock API design with trompeloeil, including deferred calls and API
   lifetime through cleanup. Acceptance: fake coverage for empty/changing enumeration, missing
   requirements, acquisition/owner-publication failures, failure bypass, exceptions, cleanup order
   and two independent runs with isolated mock expectations; a real headless instance smoke test.
   Assess template plumbing before expanding the API, and use Result-aware binding, error mapping
   and caller-selected recovery rather than duplicating failure branches.
4. **Devices and queues.** Snapshot capabilities, preserve/test scoring and queue-family policy,
   create the logical device and retrieve queues. Introduce surface-dependent suitability only once
   the platform boundary is proven. Acceptance: unsuitable/missing requirements, split queue
   families, ties and no device; failure bypasses later work; parent/child cleanup traces; real
   device create/destroy smoke coverage. Keep external synchronization explicit.
5. **Platform resources and a bounded frame.** Port window/surface, swapchain/views, render pass,
   framebuffers, command pools/buffers and semaphores/fences as the actual demo requires. Keep
   format/extent/image-count decisions pure. Chain acquire -> record -> submit -> present with
   command-specific status handling. Acceptance: fake frame ordering, host synchronization,
   generation and recoverable-status checks; a bounded real windowed run clears/presents without
   validation errors. Add shaders/pipelines/geometry only for an explicitly selected further demo.
6. **Lifecycle and replacement.** Compose event handling, minimize/zero extent, resize,
   out-of-date/suboptimal recreation, frame-slot changes, device-loss failure and shutdown. Keep old
   resource generations alive until no longer in use and reject stale actions. Acceptance: scripted
   recovery/failure scenarios plus a real resize/recreation/close run that terminates cleanly.
   Remove superseded imperative paths only after the command inventory demonstrates parity.
7. **Regression and extensibility.** Keep pure, mocked impure and real integration tests tagged as
   specified below. Run all tests for now, with unavailable integration prerequisites reported as
   skips. Retain a small diagnostic suite and the consumer integration check. Record tested
   platform/driver requirements. Confirm that components can compose another demo without copying a
   monolithic setup/rendering function. Broader library packaging or new scheduler architectures are
   separate work unless a concrete slice requires them.

## Test and review checklist

Keep embedded doctests at the end of their corresponding `.cpp` files. Every test-case name must
contain exactly one category tag: `[pure]` for policy/data transformations, `[impure]` for actions
and orchestration using mocked APIs, or `[integration]` for real-API smoke and regression tests. Aim
for most coverage to be pure, with focused mocked effect tests and a smaller integration suite.
Every integration test must additionally have exactly one of `[headless]` (no window/display) or
`[windowed]` (requires a window/display). Headless Vulkan tests still need a working Vulkan
implementation; the tag does not promise that every CI machine can run them.

Run all tests for now. Manual doctest filtering can exclude integration tests with
`--test-case-exclude="*[integration]*"`. There is no CI setup at present; defer default test-set
selection and CMake/CTest filtering plumbing. A future CMake configuration variable may make CTest
exclude integration tests. Filtering controls execution, not linkage: pure and mocked impure tests
must avoid real API calls but may link Vulkan and SDL.

| Tier                     | Required evidence                                                                                                                                                                                      |
| ------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Pure doctests            | Owning names/snapshots; capability and device/queue policy; swapchain decisions; frame/lifecycle state transitions; result classification.                                                             |
| Fake dispatch            | Laziness, calls/arguments, bounded enumeration, failure bypass, partial acquisition/owner-publication exceptions, parent lifetime, reverse cleanup, repeat runs, stale generations, no unsafe overlap. |
| Execution spike          | Main-thread identity at platform calls and cleanup, re-dispatch after suspension, concurrent wakeups, launcher/loop exceptions, quit while work remains, join/finalize ordering.                       |
| Compile-time diagnostics | Positive counterparts and a small set of negative callback/state/capability/non-monad boundaries with useful names and reasons.                                                                        |
| Headless real Vulkan     | Instance/debug setup, device/queue setup and deterministic teardown; validation enabled where available.                                                                                               |
| Windowed real Vulkan     | Bounded clear/present, synchronization, minimize/resize/recreation and close; no validation errors or hangs.                                                                                           |
| fnfelt changes           | Library DEV/regression gates plus the downstream consumer; forwarding, error/state retention, lifetime and diagnostic contracts of each new primitive.                                                 |

Review each slice for hidden effects in pure functions/destructors, escaped borrowed storage,
unnamed composition boundaries, ignored statuses, resource release before CPU/GPU completion,
main-thread assumptions and accidental parallel host access. Do not extrapolate success on one GPU
to a portable synchronization guarantee.

## Next session and working practices

SDL video initialization, window ownership, required instance-extension discovery and configurable
Vulkan instance creation are implemented. Main-owned resource holders retain cleanup guards while
worker ReaderIO programs borrow views. Lexical destruction releases the window before video. Mocked
tests cover errors, ownership and main-thread calls; windowed integration tests cover SDL
initialization, a hidden Vulkan window, extension discovery and Vulkan instance creation with those
extensions. No Vulkan surface is created yet: pause here for review before adding surface creation
and ownership. Keep the legacy implementation static as a reference until migration is complete.

The test runtime uses fnfelt's `io::on` and a main-thread libfork dispatch loop. Background
`sync_wait` completion sets an atomic flag and wakes that loop outside the ReaderIO environment.
Joining the worker precedes releasing SDL resources on main and finalizing its context. Application
event polling, cooperative cancellation and the production runtime remain future work.

Read `AGENTS.md`, this plan, the legacy responsibilities and fnfelt's main-thread demo when
selecting exact APIs. Load the reflection skill before reflection code and the Immer skill before
changing Immer behavior. Use subagents for targeted exploration, implementation and review rather
than loading every wrapper into one context.

Settle bounded open questions with evidence or the user: minimum supported Vulkan/platform version,
validation policy, first supported device/queue topology, exact state/environment shape, concrete
resource ownership and polling/wakeup bounds. The initial main-loop topology is already selected;
its application integration and shutdown details still need proof. Settle cleanup for each resource
slice rather than postponing it until the final loop or introducing a generic session in advance.

Keep atomic Conventional Commits focused on responsibilities and minimize overlapping files across
commits so future fixups/rebases stay manageable. Use `fixup!` commits when refining an existing
change. The user reviews and autosquashes; do not rebase unless explicitly asked. Preserve unrelated
working-tree edits. Do not update README or `build_env/flake.nix` without the authorization required
by current guidance. Avoid docstring edits to unrelated functions and broad renaming/reformatting
outside the slice being migrated. Update this plan as contracts are proven and phases complete.
