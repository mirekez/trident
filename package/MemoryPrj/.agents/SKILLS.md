# Trident GUI Push Commands

This project is written in cpphdl. Before editing project sources, read the cpphdl coding rules and headers in:

```text
/home/me/trident/build/tools/build-current/cpphdl/include
```

Backend variable values available to agents:

```text
$(BinDir)=/home/me/trident/build
$(ToolsPath)=/home/me/trident/build/tools/build-current
cpphdl_tool=/home/me/trident/build/tools/build-current/cpphdl
```

ALWAYS PUSH NEW SOURCES YOU CREATE! Use the backend push gateway to request GUI actions from scripts or agents.

Current push folder:

```text
/home/me/trident/build/.push
```

Create one JSON command file in that folder. The backend consumes the file, streams the command to the GUI, and deletes the file after sending it.

File naming:

```text
pushSource_<incremented_cmd_id>.json
pushProjectSettings_<incremented_cmd_id>.json
```

Use a monotonically increasing numeric command id. Example:

```text
/home/me/trident/build/.push/pushSource_1.json
/home/me/trident/build/.push/pushProjectSettings_2.json
```

Supported push functions:

- `pushSource(filename)`: asks the GUI to open `filename` in `Development -> EditorTabs`.
- `pushProjectSettings(topModuleName, topModuleFile, mainTestFile, additionalSources)`: asks the GUI to save project settings in the backend.

`pushSource` JSON format:

```json
{"filename":"/absolute/path/to/source.cpp"}
```

`pushProjectSettings` JSON format:

```json
{"topModuleName":"Memory","topModuleFile":"Memory.cpp","mainTestFile":"MemoryTest.cpp","additionalSources":"MemoryHelper.cpp"}
```

Parameter rules:

- `filename` must be an absolute or project-root-accessible source path.
- The file must be readable by the backend and allowed by the active project root.
- The Development window must be open in the GUI; otherwise the GUI ignores the command and reports status.

After generating the cpphdl model source and the main test source, push the project settings with `pushProjectSettings` so Trident knows the top module name, top module file, main test file, and additional sources.

Shell example:

```sh
printf '{"filename":"/path/to/file.cpp"}\n' > /home/me/trident/build/.push/pushSource_1.json
printf '{"topModuleName":"Memory","topModuleFile":"Memory.cpp","mainTestFile":"MemoryTest.cpp","additionalSources":"MemoryHelper.cpp"}\n' > /home/me/trident/build/.push/pushProjectSettings_2.json
```
