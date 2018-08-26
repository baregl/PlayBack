use byteorder::{LittleEndian, ReadBytesExt};
use failure;
use failure::ResultExt;
use std::ffi::OsString;
use std::io::Cursor;
use std::os::unix::ffi::OsStringExt;
use std::path::PathBuf;

pub const HEADER_SIZE: u32 = 36;
pub const ENTRY_SIZE: u32 = 272;
pub const QUEUE_MAX_DEPTH: u32 = 64;
pub const TRANSFER_REQ_SIZE: u32 = 257;
pub const TRANSFER_SIZE: u32 = 8192;
pub const CONFIG_SIZE: u32 = 2048;
pub const REQUEST_SIZE: u32 = 257;
// This needs to be a multiple of 4 for the hash
pub const READ_SIZE: u32 = 1048576;

#[derive(PartialEq, Debug)]
pub struct Header {
    pub name: String,
    pub id: String,
    pub version: String,
    pub pass: u32,
}

impl Header {
    // buffer needs 36 Elements
    pub fn get(buffer: &[u8]) -> Option<Self> {
        if buffer.len() != 36 {
            return None;
        }
        if !buffer.starts_with("PLYSYNC1".as_bytes()) {
            return None;
        }
        let mut name = buffer[8..16].to_vec();
        name.retain(|&x| x != 0);
        let name = String::from_utf8(name).ok()?;
        let mut id = buffer[16..24].to_vec();
        id.retain(|&x| x != 0);
        let id = String::from_utf8(id).ok()?;
        let mut version = buffer[24..32].to_vec();
        version.retain(|&x| x != 0);
        let version = String::from_utf8(version).ok()?;
        let pass = Cursor::new(&buffer[32..36])
            .read_u32::<LittleEndian>()
            .ok()?;
        Some(Header {
            name,
            id,
            version,
            pass,
        })
    }
}

#[derive(PartialEq, Debug)]
pub enum EntryType {
    File,
    Directory,
    End,
}

#[derive(PartialEq, Debug)]
pub struct Entry {
    pub entry_type: EntryType,
    pub path: PathBuf,
    pub size: u32,
    pub mtime: u64,
}

impl Entry {
    // buffer needs to have 272 elements
    pub fn get(buffer: &[u8]) -> Result<Self, failure::Error> {
        ensure!(
            buffer.len() == ENTRY_SIZE as usize,
            "Entry buffer length doesn't match"
        );
        let entry_type = match buffer[0] as char {
            'f' => EntryType::File,
            'd' => EntryType::Directory,
            'e' => EntryType::End,
            _ => bail!("Unknown entry type"),
        };
        let size = Cursor::new(&buffer[4..8])
            .read_u32::<LittleEndian>()
            .context("Parsing size")?;
        let mtime = Cursor::new(&buffer[8..16])
            .read_u64::<LittleEndian>()
            .context("Parsing mtime")?;
        let mut path = buffer[16..272].to_vec();
        path.retain(|&x| x != 0);
        let path = PathBuf::from(OsString::from_vec(path));
        Ok(Entry {
            entry_type,
            path,
            size,
            mtime,
        })
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn basic_utf8_conversion_test() {
        let buf = ['H' as u8, 'I' as u8, 0, 0, 0, 0];
        let mut name = buf.to_vec();
        name.retain(|&x| x != 0);
        let name = String::from_utf8(name).unwrap();
        assert_eq!(&name, "HI");
    }
    #[test]
    fn header_parsing() {
        use super::*;
        let mut data = String::from("PLYSYNC13ds\0\0\0\0\0SlimBlue0.0.0\0\0\0").into_bytes();
        data.push(0xAB);
        data.push(0xCD);
        data.push(0xEF);
        data.push(0x05);
        assert_eq!(
            Header::get(&data).unwrap(),
            Header {
                name: "3ds".to_string(),
                id: "SlimBlue".to_string(),
                version: "0.0.0".to_string(),
                pass: 0x05EFCDAB,
            }
        );
    }
}
