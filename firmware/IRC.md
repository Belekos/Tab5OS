# IRC chat

Use firmware **0.8.19** with its matching SD modules. From the home screen, open **Python applications → IRC chat**.

Choose a server, enter your nickname and, optionally, a channel beginning with `#`. **Connect** opens the connection; the application never connects on its own. The last server and nickname are remembered. **Options** lets you enter a custom server, change the saved nickname or clear recent entries. The nickname is saved locally; this does not create a NickServ account on the network.

The suggested servers use the TLS endpoints published by their operators:

| Network | Server | TLS port |
|---|---|---|
| Libera Chat | `irc.libera.chat` | 6697 |
| OFTC | `irc.oftc.net` | 6697 |
| hackint | `irc.hackint.org` | 6697 |

Libera was validated on the tablet by the user. The other two addresses were checked against their official documentation; access depends on the network and its rules. The tablet's date must be correct for certificate verification. The eight most recently used servers appear in the list; choosing a server does not start a connection.

The **Channels** button opens channel management while connected:

- **Active**, **Favorites** and **Recent** show channels for the current server. Choosing a name fills the field without automatically joining the channel.
- **Join** opens the entered channel and returns to the conversation.
- **Add to favorites** and **Remove from favorites** change the favorites without joining or leaving a channel.
- **Leave channel** sends PART and closes the conversation. Its favorite entry, if any, remains saved.

The sixteen favorites and sixteen recent channels are associated with the server and port. A connection supports up to eight simultaneous conversations. Channels are added to recent entries when the server confirms that they have been joined. Favorites are never joined automatically. PING/PONG continues on the Channels page and while the application is hidden.

Enter text, then press **Enter** on the USB or touch keyboard, or choose **Send**. The input remains open for the next message. The top list switches conversations; **Server** displays IRC responses and errors. The arrows move back through history or return to recent messages.

| Command | Action |
|---|---|
| `/join #channel` | Join a channel |
| `/msg nickname text` | Send a private message |
| `/me text` | Send an action in the active conversation |
| `/nick new_nickname` | Change nickname; the nickname confirmed by the server will be saved |
| `/part` | Leave and close the active channel |
| `/close` | Close the conversation, clear its history and leave the channel if applicable |
| `/quit` | Disconnect |
| `/help` | Show the commands |

**Hide** keeps the connection open; resume the application through its icon or **Ctrl+Alt+P**. **Stop**, or **Ctrl+.**, terminates the script. Only one Python application runs at a time; native applications can continue as resources permit.

Preferences are stored in `/System/irc.json`, using journaled replacement on SD. Changes received from the server are grouped before writing; an SD error leaves the conversation running and displays the save failure. Once the SD card is working again, **Options → Save** retries the operation. Messages are not saved. Total in-memory history is limited to 128 protocol lines; long lines can occupy several display lines. The send queue holds eight messages, spaced at least 500 ms apart; displaying a sent message is not a read receipt. PONG responses take priority over this queue.

The connection uses **TLS with verified certificates** exclusively. This version does not support automatic SASL authentication, server passwords, DCC or file transfers. A network or channel may require an authenticated account. There is no automatic reconnection after a disconnection.

Sources checked on 27 September 2026: [Libera.Chat](https://libera.chat/guides/connect), [OFTC](https://www.oftc.net/) and [hackint](https://www.hackint.org/connect).
