# Network tools

The **Terminal** and the **Pentest** ELF application use the same network operations supervised by the OS. Wi-Fi must already be connected. Open Pentest from **ELF applications > Pentest**; its ELF file and launch profile must be on the SD card. See [PENTEST.md](PENTEST.md). Built-in help describes each parameter; the complete reference is in [TERMINAL.md](TERMINAL.md).

| Command | Purpose |
|---|---|
| `netinfo`, `ifconfig`, `ip addr`, `ip route` | Current configuration: address, subnet mask, gateway, DNS and signal |
| `dns host`, `nslookup host` | Resolve a name to IPv4 |
| `ping -c 3 -W 0.5 host` | Three ICMP requests, with a 0.5-second timeout per reply |
| `tcp -w 2 host 443`, `nc -z -w 2 host 443` | Test a TCP connection |
| `ports -w 0.35 host 22,80,443,8000-8010` | Test explicitly selected ports, up to 1,024 distinct ports |
| `headers -L https://example.org` | HTTP headers, TLS validation and observations about advertised protections |
| `wget -L -O usb:/Documents/page.html URL` | Download to USB, following redirects |
| `curl -L -o sd:/Documents/page.html URL` | Download to SD; without `-o`, output is limited to 32 KiB |

For HTTP transfers, `-T` sets a total duration of 1 to 3,600 seconds, with a default of 120. Connection and read waits have separate limits. Cancellation or a timeout may have to wait for the current DNS resolution or connection attempt to finish. The system date must be correct for HTTPS. The certificate and server name are verified; redirects from HTTPS back to HTTP are rejected. An existing file is preserved if the transfer fails or is cancelled.

## Pentest application

The home screen provides a dedicated application. Choose an IP address or hostname, then the analysis type:

- **Overview**: DNS resolution, four ping attempts and a configurable list of TCP ports.
- **DNS**, **Network response** or **TCP ports**: run only the selected operation.
- **HTTP and TLS**: enter a URL. The report shows the status and headers, verifies TLS for HTTPS and notes issues such as missing HSTS, CSP or X-Content-Type-Options. Interpret these observations in the context of the service; they do not by themselves prove a vulnerability.

**Ctrl+Enter** starts an analysis, **Ctrl+.** stops it and **Ctrl+S** opens the destination folder picker for exporting the report. **Ctrl+L**, the trash icon and **right-click > Clear output** clear the displayed report without stopping an active analysis or deleting a previously saved export. Report text can be selected and copied using the standard editing commands. Reports receive a unique `pentest-*.txt` name on SD or USB.

An open port means that a connection was accepted. A timeout does not distinguish a firewall from an unreachable device. The current application does not provide general radio capture, Wi-Fi injection or an exploitation engine. Network operations remain cancellable.

Terminal network commands are described in [TERMINAL.md](TERMINAL.md). See [PENTEST.md](PENTEST.md) for the diagnostic application and [SSH.md](SSH.md) for SSH/SFTP.
