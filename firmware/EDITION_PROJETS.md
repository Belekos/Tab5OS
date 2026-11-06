# Editing, code and projects

Writing is also the code editor. Python, C/C++, Arduino, JavaScript/TypeScript, JSON, HTML/CSS, XML, YAML, TOML, scripts and common configuration files open in the same editor. Their source remains fully visible: `#`, `*`, links and tables are never interpreted as Markdown. Syntax coloring is visual only and adds no markup to the file. Markdown documents retain formatted editing, including underline and strikethrough.

The **Code** menu contains New Python script, Run, Stop, Resume, Libraries and Console. Run first saves the current version, verifies the save, then launches the saved `.py` file. A failed save cancels execution. The execution limit is 64 KiB; the editor accepts UTF-8 files up to 2 MiB. The console appears on the right, keeps at most 8 KiB and refreshes only while visible, at most four times per second.

**Documents** lists files registered in the chosen group, even when they are not loaded. The **Outside projects** group remains available. The X removes membership only; it never deletes the SD file. Switching documents first saves edits and the position. A failed save keeps the current document open.

To create a project, open a document, then choose **Documents → Create a project with this document**. Its original standalone membership is preserved. After selecting a group, use **Add document to project**. **Import folder** associates the folder with the selected project, or creates a project when none is selected. Subfolders are scanned gradually; while the editor is visible, a new scan every 30 seconds detects new files. A membership removed with X is not restored by these scans; explicitly opening the file can register it again.

Limits are 8 projects, 256 document memberships and 16 list rows per page. Paths accept 299 UTF-8 bytes. A scan visits at most 4 entries per service pass, 20,000 entries per scan and 8 subfolder levels. Hidden folders, `node_modules` and `__pycache__` are skipped. Useful configuration files such as `.env` and `.gitignore` remain accepted. Cursor, selection and scroll positions are restored per membership.

| Shortcut | Action |
|---|---|
| Ctrl+C / X / V / A | Copy / cut / paste / select all across the entire document |
| Ctrl+Insert / Shift+Insert / Shift+Delete | Copy / paste / cut |
| Insert | Toggle insertion / overwrite |
| Shift + navigation | Extend the selection |
| Ctrl+Home / End, with Shift to select | Start / end of the document |
| Ctrl+Shift+N | Line numbers, for every file type |
| Ctrl+U / Ctrl+Shift+X | Underline / strikethrough in formatted documents |
| Ctrl+Enter | Save and run the Python file |
| Ctrl+. | Stop execution |
| Ctrl+Shift+R | Resume |
| Ctrl+Shift+L or Ctrl+` | Show / hide the console |
| Ctrl+Shift+O | Open the Documents menu |
| Ctrl+Tab / Ctrl+Shift+Tab | Next / previous document in the selected group |
| Ctrl+S / Ctrl+Shift+S | Save / save as |
| Ctrl+O / Ctrl+N | Open / new document |
| Escape | Return to controls / close the menu |

Line numbers occupy a narrow gutter with a distinct background. Menus adjust their width to translations and reserve separate space for their arrow. An outline identifies the active input area. Document backups are stored in `/.tab5-backups`, following the original file's directory structure; older backups beside the original files remain recoverable. Markdown preserves underline with `<u>...</u>` and strikethrough with `~~...~~`. Source files remain plain text. Standard search, undo and editing shortcuts remain available.

The list, groups and positions are saved in `/System/editor_workspace.dat` through a verified transaction that keeps the previous version. Document recovery carries an explicit project identifier: after a power cut between two saves, an older document is never assigned to the currently selected project. Recovery is accepted only if the exact membership still exists. Removing the last document therefore prevents an old session from bringing it back. The initial package contains no personal document list.
