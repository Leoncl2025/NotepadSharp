# JSON Tools

Open **Edit > JSON > JSON Tools**. The panel is dockable and has Structure, Query, Validation, and Compare tabs.

## Paths and Navigation

- The status bar displays the node at the caret or selection and copies its JSONPath when clicked. Long paths are elided; the tooltip retains the full path.
- Right-click a key or value for Copy JSON Path, Copy JSON Pointer, Copy Key, Copy JSON Value, and Copy Decoded String. These commands use the clicked location, independently of an existing selection.
- Selecting a query result can select either the property's key token or its value. Array entries and the document root have no property key and select their value instead.
- Activate a tree row to select its source. Toolbar commands navigate to the parent or adjacent sibling, pretty-print or compact a subtree, and fold its containing object or array. Tooltips identify icon buttons.
- A path with duplicate keys is explicitly ambiguous. Exact copied paths and JSON Pointers can locate all occurrences; general JSONPath expressions skip ambiguous records rather than guess which value a filter should use.
- Paths do not modify the document. A serialized JSON string remains a string until an explicit formatting or preview operation decodes it.

## Queries

**Find JSON Path** is Cmd+Option+P on macOS or Ctrl+Alt+P on Windows. The Path mode accepts jsoncons 1.9 JSONPath expressions or nonempty JSON Pointers:

| Expression | Result |
| --- | --- |
| `$.users[0].name` | First user's name |
| `$.users[*].name` | Every user's name |
| `$..name` | Recursively find name properties |
| `$.users[?(@.age >= 18)].name` | Names of adult users |
| `$["a.b"]["/~"]` | Literal punctuation in property names |
| `/users/0/name` | JSON Pointer equivalent |
| `/a.b/~1~0` | JSON Pointer with escaped slash and tilde |

Use Key or Value mode for substring searches, with optional case sensitivity. Results show line, path, type, and value. Copy Results in the Query tab copies full paths and raw values as tab-separated text, not the shortened on-screen previews.

## Validation

Syntax errors and duplicate-key warnings update after editing. Clicking an issue moves to its source position. When syntax is damaged, only recognizable nodes receive paths; the tool does not invent missing properties or repair syntax.

Select a local JSON Schema file and run Validate JSON. The engine supports JSON Schema dialects including Draft 7 and 2020-12. Leave the schema filename empty for syntax/duplicate-key checking only. References within the supplied schema, including `$defs`, work. External references are rejected: the application does not download schemas or follow arbitrary local filesystem references.

## Structural Comparison

Choose another local JSON file in Compare. The current, possibly unsaved editor buffer is the Before side; the selected file is the After side. Added, removed, modified, and optionally reordered object keys are listed by path. Arrays are compared by index. Equivalent numeric spellings such as `1` and `1.0` compare equally.

Click a Before result to select the current document's node. Click an After result to open a new unsaved snapshot with that value selected. Invalid JSON and duplicate keys must be resolved before structural comparison. Comparison result copying includes the displayed previews; it is not a JSON Patch export.

## JSON Lines

`.jsonl` and `.ndjson` filenames enable JSON Lines automatically. The panel's mode selector overrides the mode for the current editor. Each nonblank line is an independent JSON value; blank lines are ignored. Tree roots and paths identify the original physical line number.

Queries and schema validation process valid records independently. Invalid lines remain visible as issues and do not hide subsequent valid lines. A copied path is relative to one record, so querying it may return results from multiple records.

Pretty Print and Toggle open the current record in an unsaved JSON preview, preserving the original one-record-per-line buffer. Compact processes valid records in place with one undo step and leaves invalid records and line endings intact. With a selection, only fully selected records are compacted.

## Limits and Safety

Indexes are rebuilt in the background after a 180 ms editing debounce, not on every caret movement. Edits and tab changes immediately invalidate source locations and results; old background results and old context-menu actions cannot navigate or copy from a newer buffer.

The current limits are 32 MiB per document, 256 nesting levels, 500000 indexed nodes, and 10000 query/comparison results. These are bounded editor tools, not a streaming processor for arbitrarily large logs. Schema validation also limits reported issues. Query-result clipboard export is limited to 16 million characters. Raw string escapes and numeric spelling are preserved in the editor unless an explicit formatting operation is requested.

## Try It

Open [json/sample.json](json/sample.json) and query `$.users[?(@.age >= 18)].name`. The result is Alice. Copy the path of the `"/~"` value to exercise escaping. Validate against [json/schema.json](json/schema.json) to locate the underage record. Compare against [json/compare.json](json/compare.json) to see changed and added values.

Open [json/sample.jsonl](json/sample.jsonl) to exercise independent records. Its second line is intentionally malformed; `$..name` still finds names on the first and third lines.