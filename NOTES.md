# Notes
## TODO
- Add functionality to server to generate crypto keys
- Logging for server
- Save decryption & similar things
  - Transparently or just dumping all of them into a separate directory before
    syncing? Use decrypted file times when dumping
  - Also sdmc:/Nintendo 3DS/*/title
- Ideas for ports
  - Wii/Wii U/Switch
  - PS2
    - Only memory cards? Memory cards & HDD? Only HDD, assuming that everyone
      uses OPL for memory card saves? Even USB?
  - PS3/PS4
  - XBox (nxdk)/XBox 360
    - Xbox One doesn't yet have a homebrew scene
  - PSP
  - DS
  - Basically everything with network and storage
## Protocol 2
- Port 9483 with TCP
- Little Endian
### Crypto Session Beginn
Detailed in CRYPTO.md
### Begin Session (80)
- Device name (8)

  - Client (implementation) specific
  - Filled up with \0 Bytes
  - Example: "3DS\0\0\0\0\0"
  - Valid characters: ```[A-Z][a-z][0-9]```

- Client Version (8)

  - Filled up with \0 Bytes
  - Example: "0.1.3\0\0\0"
  - Valid characters: ```[0-9].```

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
  - If it's 0, force a hash check

- Path (256)

  - Filled up with \0 Bytes
  - Example: "ur0:tai/config.txt\0\0\0(...)"
  - Invalid characters: \0
  - Don't include non-real directories like ".." or ".", they will be rejected
    serverside
  - For efficient/proper server side handeling, use '/' as a directory separator.
    Otherwise *all* the files might be stored in one directory, which will lead
    to performance issues and making the data more difficult to restore.
### Data Transfer
#### Data Request
- From server
- Type (1)

  - 'f' Send file & size & hash
  - 'h' Send only hash (when mtime doesn't match)
  - 'e' End of transfer, rest filled \0 Bytes

- Path (256)
#### Data Response
Size, File & Hash are separately transmitted & encrypted
- Size (4)

  - Only if file requested

- File (size)

  - Transmitted in 2^16 Chunks
    Needed for auth
  - Only if file requested

- Hash (4)

  - Murmur3 hash of file contents
