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
- Client C library for most of the logic
- Linux client as a reference
- Vita client
- 3DS client
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
- Vita client
  - Vitasdk by vitasdk (MIT/GPLv3)
- 3ds client
  - libctru by Smealum (MIT)
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
## Known Issues
- The 3DS client takes really long for directories with lots of files. This
  seems to be an inherent limitation of the 3ds.
- The 3DS client handling of the home menu/power menu still has some issues
