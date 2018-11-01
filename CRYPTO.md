# Reasons for encryption
- Sometimes there's sensitive Data on consoles (sign-in keys, photos etc.)
- While a local network is way better than the open internet, it's still
  desirable to be able to use the internet as a way to securely sync
- In case of a future bidirectional sync, by simply passivly sniffing the
  password hash, you can always syphon data from the server
# Reasons why (unfortunately) TLS won't work
- TLS requires a valid security certificate, which, while significantly easier
  in the world of lets encrypt is still a daunting task for most people.
  Self-signed certificates would require significant UI structure, especially as
  they'd need to be changed when they're not valid anymore
- Many consoles don't implement TLS/a current Version of TLS (3ds, wii, many
  others), requiring a port of an ssl library with all it's overhead, which is
  pretty involved. Especially since the most mature codebases (openssl) are
  horrible in many aspects and complicate integration in simple makefile
  buildsystems.
# Overview
- For all cryptograhic functions, TweetNaCL is used
  - It's pretty easy to integrate, as it's only a single c file
  - It has no architecture specific code, so no porting effort
  - It's by a renowned cryptographer (djb) and seem to be cryptographically
    vetted
  - API is hard to screw up (except the 0-padding, wtf)
- For communication, "crypto_secretbox" is used, with the key beeing the sha512
  hash of the password and providing authenticated encryption
- As a protection against replay attacks a custom nonce exchange system is used.
  This is the most vulnerable, as it's been not vetted by any cryptographer.

- If you know any better way that satifies the constaints of beeing simple, easy
  to integrate and portable, please let me know
# Nonce exchange
- /Important/ The random numbers from the client are probably not very random,
  so don't solely rely on them

1. The client sends the magic "PLYSYNC2" & client id to the server
2. The server generates two random nonces a & b, and transmitts a message
   containing b encrypted with the password & a, sending a in plaintext
   before the message
3. The client appends a random value to nonce b, hashes it with sha512, and
   takes the first NONCEBYTES with the least significant bit being zero, sends a message
   containing the resulting nonce c encrypted with b.
   
   (Now the server can trust the client)
4. The server sends a message containing "OK" with the nonce c
   
   (Now the client can trust the server [given a random enough generated value])
5. All further communication simply increments the previous nonce by two, server
   uses even, client uses the odd nonces

# Library notes
With tweetnacl the first 16 Bytes of cipher/plaintext are always zero and don't
need to be transmitted. The second 16 bytes seem to be used for authentication.
The cryptobox_easy functions from libsodium seem to use the same format as
tweetnacl, but without the first 16 Bytes (and the authentication removed from
plaintext.

We don't send the first BOXZEROBYTES (16)
