# PlayBack
*Warning*: This is alpha-level software, not ready for anyone to use it.
If you're adventurous, you can try it, but please make sure:
1. That there are no sensitive files containing account data or something
   similar on your console
2. That you run the server under a user with minimal access to anything/in a
   vm/in a container, so that attackers that may be able to override files will
   not be able to do much harm
## Intro
You should regularly make backups. For Everything. But for hacked consoles it
isn't always that easy. In lots of cases (Vita, Original XBox, PS2), their files
are basically only available via FTP, which can be slow and has to be driven by
a computer. PlayBack is an easily implementable protocol, which can backup your
device to a server. Depending on the client, device-encrypted files may even be
transparently decrypted, so you can continue playing on a seperate console.

Available Clients:
- Client C library, just implement the callback functions and you're ready to go
- Reference unix client
Available Servers:
- Reference server, Rust based (Unix, may have serious path traversal bugs on
  Windows or similar things)

It works over a non-encrypted TCP connection to Port 9483. Your password is
hashed, but if you're on an untrustworthy network, all your files can be leaked.
You can only change or upload new files to the server, not read existing ones.

## Based on
- Clientlib
  - PKGJ for the config parser by Philippe Daouadi (BSD 2-Clause)
  - Murmur3 by Austin Appleby from qLibc ported by Seungyoung Kim (BSD 2-Clause)
- Server
  - Rust, so I'm reasonably sure it doesn't have RCE Bugs
  - Crates:
    - Log by The Rust Project Developers (MIT/Apache-2.0)
    - Simplelog by Victor Brekenfeld (MIT/Apache-2.0)
    - Failure by Without Boats (MIT/Apache-2.0)
    - Structopt by Guillaume Pinot (MIT/Apache-2.0)
    - Murmur3 by Stu Small (MIT/Apache-2.0)
    - Byteorder by Andrew Gallant (Unlicense/MIT)
    - Filetime by Alex Crichton (MIT/Apache-2.0)
    - Toml by Alex Crichton (MIT/Apache-2.0)
    - Serve by David Tolnay & Erick Tryzelaar (MIT/Apache-2.0)
## License
Everything is licensed under AGPL, found in the LICENSE file, except files
with their own copyright header.
## TODO
- Add 3DS & Vita clients
- Add status output to client
- Add logging to server
## Protocol
- Port 9483 with TCP
- Little Endian
### Begin Session (36)
- Magic string "PLYSYNC1" (8)
- Device name (8)

  - Client specific
  - Filled up with \0 Bytes
  - Example: "3DS\0\0\0\0\0"
  - Valid characters: ```[A-Z][a-z][0-9]```

- Device id (8)

  - Client device specific
  - Filled up with \0 Bytes
  - Example: "SlimBlue"
  - Valid characters: ```[A-Z][a-z][0-9] ``` (space)

- Client Version (8)

  - Filled up with \0 Bytes
  - Example: "0.1.3\0\0\0"
  - Valid characters: ```[0-9].```

- Password (4)

  - Murmur3 hash of the password

### File Listing (272)
- Type (1)

  - 'f' File
  - 'd' Directory, size is 0, mtime of 0 is also valid
  - 'e' End of list, rest filled with \0

- Filler "\0\0\0" (3)
- Size in Bytes (4)
- Modification Time in unix Time (8)

  - Doesn't have to be exact (eg. Timezones/leap seconds)
  - Must be consistent (no fluctuations)
  - Should roughly match (~day)

- Path (256)

  - Filled up with \0 Bytes
  - Example: "ur0:tai/config.txt\0\0\0(...)"
  - Invalid characters: \0
  - Don't include non-real directories like ".." or ".", they will be rejected
    serverside
  - For efficient/proper server side handeling, use '/' as a directory seperator.
    Otherwise *all* the files might be stored in one directory, which will lead
    to performance issues and be very to restore.
### Data Transfer
#### Data Request
- From server
- Type (1)

  - 'f' Send file & size & hash
  - 'h' Send only hash (when mtime doesn't match)
  - 'e' End of transfer, rest filled \0 Bytes

- Path (256)
#### Data Response
- Size (4)

  - Only if file requested

- File (size)

  - Only if file requested

- Hash (4)

  - Murmur3 hash of file contents
