use super::{config, parse};
use failure::{Error, ResultExt};
use sodiumoxide;
use sodiumoxide::crypto::{hash, secretbox};
use std::collections::HashMap;
use std::io::{Read, Write};
use std::net::TcpStream;

pub const ID_SIZE: usize = 8;

pub struct Encrypted {
    stream: TcpStream,
    key: secretbox::Key,
    snonce: secretbox::Nonce,
    cnonce: secretbox::Nonce,
}

impl Encrypted {
    pub fn begin_encrypted_communication(
        mut stream: TcpStream,
        devices: &HashMap<String, config::Device>,
    ) -> Result<(Self, String), Error> {
        sodiumoxide::init().expect("Couldn't init sodiumoxide");
        let mut crypt_header = [0; parse::CRYPT_HEADER_SIZE];
        stream
            .read_exact(&mut crypt_header)
            .context("Couldn't read crypto header")?;
        let crypt_header = parse::CryptHeader::parse(&crypt_header)
            .map_err(|err| {
                format_err!(
                    "Failed
            to parse crypt header: {}",
                    err
                )
            })?.1;
        let key = if let Some(device) = devices.get(&crypt_header.id) {
            let digest = hash::hash(&device.key.as_bytes());
            // the hash is always larger than the key
            secretbox::Key::from_slice(&digest[0..secretbox::KEYBYTES])
                .expect("The hash is always larger than the key")
        } else {
            bail!("Device {} isn't registered", &crypt_header.id);
        };

        let noncea = secretbox::gen_nonce();
        let nonceb = secretbox::gen_nonce();
        stream
            .write(noncea.as_ref())
            .context("Couldn't write unencrypted nonce")?;
        let enc = secretbox::seal(nonceb.as_ref(), &noncea, &key);
        stream
            .write(&enc)
            .context("Couldn't write encrypted nonce")?;

        let mut cryptnonce = [0; secretbox::NONCEBYTES + secretbox::MACBYTES];
        stream
            .read_exact(&mut cryptnonce)
            .context("Couldn't read nonce")?;
        let noncec = secretbox::open(&cryptnonce, &nonceb, &key)
            .map_err(|_| format_err!("Failed to decrypt nonce"))?;
        let snonce = secretbox::Nonce::from_slice(&noncec).expect("The length should always match");
        let cnonce = snonce.increment_le();
        let enc = secretbox::seal(b"OK", &snonce, &key);
        stream
            .write(&enc)
            .context("Couldn't write final crypto OK")?;
        Ok((
            Encrypted {
                stream,
                key,
                snonce,
                cnonce,
            },
            crypt_header.id,
        ))
    }

    pub fn send(&mut self, data: &[u8]) -> Result<(), Error> {
        self.snonce.increment_le_inplace();
        self.snonce.increment_le_inplace();
        let data = secretbox::seal(data, &self.snonce, &self.key);
        self.stream.write(&data).context("Couldn't send data")?;
        Ok(())
    }

    pub fn receive(&mut self, size: usize) -> Result<Vec<u8>, Error> {
        self.cnonce.increment_le_inplace();
        self.cnonce.increment_le_inplace();
        let mut enc_data = vec![0; size + secretbox::MACBYTES];
        self.stream
            .read_exact(&mut enc_data)
            .context("Couldn't read data")?;
        let data = secretbox::open(&enc_data, &self.cnonce, &self.key)
            .map_err(|_| format_err!("Couldn't decrypt message"))?;
        Ok(data)
    }
}
