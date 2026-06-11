# Trident Architecture Notes

## GUI And Backend

Trident is split into a thin C++ backend and a browser-based JavaScript GUI.

The backend owns filesystem access, project state, command execution, and tool invocation. It serves the static GUI files and exposes small HTTP RPC endpoints under `/rpc/...`. RPC responses are JSON for normal data and may also include file contents or image data encoded for the browser.

The GUI is plain JavaScript in `gui/`. It does not access the filesystem directly. Controls call backend RPCs with `fetch()` through shared helpers in `app.js`, then render returned JSON into internal browser windows. The main browser page is a desktop-like workspace: the left panel opens top-level windows and the right area hosts working windows such as `Development`.

Current important RPC groups:

- Project lifecycle: create, save, load, close, settings.
- File operations: list, open, create, delete, rename, save.
- Development actions: opened tabs, log refresh, compile.
- Console actions: interactive bash stream and input.

## Top-Level GUI Actions

Top-level project actions live in `gui/app.js` and are reused from different controls instead of duplicating logic in each component.

Examples:

- The left `Save project` button calls `saveProject()`.
- The left `Load project` button opens a project archive selector and then calls `openProject(path)`.
- Double-clicking a `.trident` archive in `Filesysten.js` calls the same `openProject(path)` function through a callback.
- `Close project` calls `closeProject()`, which asks whether to save first and reuses `saveProject()`.

Lower controls receive callbacks from `app.js`:

- `Filesystem` receives `onOpenFile` and `onOpenProject`.
- `PathSelector` reports project creation through `onProjectCreated`.
- `OpenFile` reports selected files through `onOpen`.
- `Development` exposes `openFilePayload()` so external controls can add files into its `EditorTabs`.

This keeps ownership clear: controls handle local UI behavior, while `app.js` coordinates application-level state and workflows.

## Project State

The active backend project is stored in memory as `Project` in `backend/Project.h`.

Current project fields:

- `path`: root directory of the project.
- `projectName`: archive name without the `.trident` suffix.
- `topModuleName`: project setting used by compile flow variables.
- `openedFiles`: list of files that should reopen in development tabs.

Project metadata is written as JSON into:

```text
<project path>/.trident/project.json
```

When saving a project, the backend writes the JSON state, then archives the whole `.trident` directory into:

```text
<project path>/<projectName>.trident
```

When loading a project, the backend extracts the selected `.trident` archive into the archive's directory, reloads `.trident/project.json`, and makes that directory the current project root.

The project state is used by:

- File open/close tracking for `EditorTabs`.
- Project settings UI.
- Bash console working directory.
- Compile-flow variable expansion, including `$(ProjectPath)`, `$(ProjectName)`, and `${TopModuleName}`.
