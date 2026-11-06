# File sharing

No software needs to be installed on the computer. Sharing provides read and write access to files in a modern browser. It is not a USB disk or a network drive with a Windows drive letter.

1. Open **Settings > System > Sharing**.
2. Choose **Current network** if the computer and tablet are on the same network. Alternatively, disable Wi-Fi in Settings, then choose **Direct network**: the tablet creates a temporary password-protected network. Connect the computer using the network name and password displayed on the tablet.
3. Open the displayed address in the browser, for example `http://192.168.4.1:8080`, then enter the six-digit code.
4. Browse folders, download files, upload several files, create a folder, rename or delete an item.
5. Stop sharing on the tablet when finished. Sharing also stops automatically after 30 minutes, at which point the direct network disappears. The **Stop** command is visible only while sharing is active.

The direct network works without a router or Internet connection. It does not change the saved network or the automatic connection setting. Wi-Fi settings remain locked during sharing to prevent a connection loss during a copy.

Replacing a file requires confirmation. Replaced or deleted files are kept in `/.Tab5Trash/<identifier>/<original name>`. This folder is visible in the browser: you can download an earlier version and then permanently delete files you no longer need. It is never emptied automatically. Folders must be empty before deletion.

Transfers use a 16 KiB buffer without loading the entire file on the tablet. The maximum file size is 2,147,483,647 bytes (less than 2 GiB), the current limit of SDK file positions. Map volumes of 1,900 MiB remain compatible. Incomplete uploads are cleaned up after an ordinary interruption; after a power loss, a `.t5upload-*` file may remain in the destination folder. This temporary file can be deleted. A failure during replacement may leave the previous version only in `/.Tab5Trash`, where it remains recoverable.

Final changes wait for a period without incompatible SD activity. If the card stays busy, close the current document or stop recording, then try again. A network radio can remain active. Only one file request is processed at a time; avoid starting a download and an upload simultaneously. HTTP sharing is restricted to the selected local subnet. It is enabled only on request and requires the code displayed on the tablet.
