# Serial port

**Settings > System > Serial** selects **USB-C** or **RS485**, then **Debug** or **Data** mode. Apply saves the settings to NVS only when they change. The initial setting is USB-C / Debug. There are no periodic writes to microSD.

- **USB-C**: the P4's integrated USB Serial/JTAG device, connected to a computer with a data cable. Open its serial port in the PC software. Baud rate and UART framing do not apply to this USB connection; those fields are disabled.
- **RS485**: the Tab5's built-in RS485 connector, UART1, TX GPIO20, RX GPIO21 and direction control GPIO34. Do not connect RS232 or TTL UART directly to the A/B lines. Baud rates range from 1,200 to 921,600; supported formats are 8N1, 8E1, 8O1 and 8N2. The terminal command also accepts an intermediate baud rate.
- **Debug**: mirrors application logs and stdout/stderr to the selected port. The diagnostic UART0 remains available separately. On the selected port, the `m` key requests memory diagnostics only in Debug mode; UART0 retains this command in both modes.
- **Data**: sends and receives bytes using the commands below. Ordinary application and SDK logs are excluded from this connection. Boot ROM and fatal-error messages may still appear during a restart: an external protocol must wait for startup and provide its own framing.

USB-A remains the existing host for gamepads, keyboards, mice, storage and RTL-SDR. The serial service does not install a second host or reconfigure the high-speed USB-A controller. It does not use TinyUSB CDC: the hardware USB Serial/JTAG device uses the full-speed USB-C PHY.

## Terminal

```sh
serial                         # status, RX/TX counters, queues and dropped bytes
serial port usb                # or rs485
serial mode data               # or debug
serial baud 115200             # RS485 only
serial frame 8N1               # RS485: also 8E1, 8O1, 8N2
serial ending crlf             # none, lf, crlf
serial send 'Bonjour'          # text + configured line ending
serial write 'Bonjour'         # exact text, no added line ending
cat /Documents/message.txt | serial write
serial write < /Documents/message.txt
serial read 4096 1000 > /Documents/reception.bin
serial listen 60000            # listen for 60 seconds; Ctrl+C interrupts
serial clear                   # cancel pending queues
```

`read [bytes [milliseconds]]` receives up to the specified number of bytes within the given timeout: 8,192 bytes / 1,000 ms by default. A timeout of 0 immediately drains the bytes already available. `listen [milliseconds]` continues until the timeout (60 seconds by default, at most one hour). Redirections preserve binary bytes; Terminal display is intended for text. `serial write` without arguments reads its standard input. Sending proceeds incrementally and can be interrupted; if the queue stops progressing for 10 seconds, the command reports failure. Success means that the bytes were accepted by the local queues, not that a remote device acknowledged them.

Select Data mode before sending. Setting changes are refused while an application transmission is pending. `clear` or **Cancel transmission** cancels what remains in the application queues; bytes already handed to the driver or device cannot be withdrawn.

## Editor

In the editor, **View > Serial** opens or collapses the pane beside the text. The menu entry is checked while the pane is visible. There is no longer a separate Serial button in the toolbar.

1. Choose USB-C or RS485 and its settings in **Settings > System > Serial**.
2. Open **View > Serial** in the editor. If needed, **Data mode** enables communication without application logs.
3. **Send** uses the selection. Without a selection, it sends the entire document, including off-screen passages. The source line indicates Selection or Entire document and the byte count.
4. **Listen** receives a text stream in the pane. **Stop** releases reception. **Insert** places the received text at the cursor or replaces the selection; the document is not changed automatically.
5. **Clear** empties only the received text. **Cancel** interrupts the editor's transmission.

Sending copies a snapshot into PSRAM, up to the 2 MiB document limit: subsequent document edits do not change bytes already prepared. UTF-8 Markdown content and its line endings remain unchanged; no line ending is added. The clipboard is preserved. Unavailable actions are disabled: sending an empty document, stopping an inactive listener, cancelling when nothing is being sent, or inserting/clearing an empty receive buffer. **Data mode** disappears once that mode is active. Buttons adapt to the width of translated text; an overlong row wraps to the next line without overflowing the pane.

Collapsing the pane keeps listening active; the **View** menu remains highlighted while receiving or sending. **Close the right pane** also collapses this pane without interrupting reception. Leaving the editor ends its receive session and cancels the remainder of its transmission. Reception has a single reader: if the terminal is already listening, the editor reports that the serial port is busy. The editor does not interrupt a transmission owned by the terminal.

The pane is intended for text: CRLF and CR appear as LF line breaks. Non-text bytes and invalid UTF-8 sequences are reported and replaced for display. Reception keeps up to 16 KiB of the most recent data and reports truncation. To keep the interface responsive, the pane displays at most the latest 4 KiB without splitting a UTF-8 character; **Partial display** indicates this. **Insert** uses the entire retained buffer, not just this visible window. To preserve binary bytes, use `serial read` or `serial listen` with redirection in the terminal. File transmission also remains available through `serial write < /Documents/message.txt`.
