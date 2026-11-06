# Explorer

The left pane is a **folder** tree. The right pane shows the files and folders in the open directory, or a related view. The selection determines which items commands affect; moving the focus alone does not change their target.

## Startup and storage

The **SD** root opens on first access, or **USB** if no SD card is available. **USB** appears as another volume in the tree only while a volume is mounted. Navigation is kept in memory while the OS is running: leaving Explorer and returning does not always reset it to the root. This state is not stored as a new persistent preference between restarts.

## Gestures

| Gesture | Result |
|---|---|
| Click a folder on the left | Select the folder and display its contents on the right |
| Click a folder or file on the right | Select it only; the displayed list and path still refer to the open folder |
| Double-click a folder on the right | Open the folder in the right pane and reveal/focus the corresponding folder on the left |
| Double-click a file on the right | Open it with the appropriate application; an unrecognized file is not executed arbitrarily |
| Click a folder's chevron on the left | Expand/collapse that branch only |
| Ctrl/Shift or multiple selection | Change the selection without navigating |
| Right-click | Open the menu for the selection or the explicitly targeted item |

A chevron appears only after a subfolder has been detected. Detection runs progressively for rows needed on screen and does not recursively scan the entire volume at startup. A folder containing only files has no chevron.

The path in the right-hand header identifies the displayed contents. Selecting a row does not replace the open folder's path with that row's path. A thin line separates the two panes.

## Menus

- **File**: create the types of items offered by the OS.
- **Edit**: copy, cut, paste, rename, delete and manage the selection.
- **Actions**: open an item, choose an application, view information and access actions suited to the selected type.
- **Git**: a contextual submenu of Actions or the selection menu, adapted to the chosen folder; an ordinary folder and a repository offer different operations. The Git shortcut is visible only for a folder recognized as a Git repository.
- **View**: presentation options and hidden files.
- **Sort icon**: choose the sort order.
- **Magnifying glass**: search. Select SD or USB directly in the tree.

Commands that modify items act on the explicit selection. With no selection, pressing Delete in Explorer does not target a document open in another application. Cancelling a deletion preserves the contents being viewed. Creation and paste destinations use the chosen context; after navigating to Parent, they follow the displayed folder.

## Names and validation

Suggested file and folder names are in English, regardless of the interface language. Existing personal files and configured paths are not renamed automatically. Recognized legacy names remain available as compatibility paths where needed.
