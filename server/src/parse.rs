use nom;
use nom::{le_u32, le_u64};
use std::ffi::OsString;
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
pub const READ_SIZE: u32 = 1_048_576;

fn parse_limited(input: &[u8], size: usize) -> nom::IResult<&[u8], Vec<u8>> {
    if input.len() < size {
        Err(nom::Err::Incomplete(nom::Needed::Size(size - input.len())))
    } else if size == 0 {
        error!("parse_limited_string size is 0");
        Err(nom::Err::Error(error_position!(
            input,
            nom::ErrorKind::Custom(0x15E0)
        )))
    } else {
        let (data, rest) = input.split_at(size);
        let data = data.split(|&x| x == 0).next().unwrap().to_vec();
        Ok((rest, data))
    }
}

fn parse_limited_string(input: &[u8], size: usize) -> nom::IResult<&[u8], String> {
    let ret = parse_limited(input, size);
    match ret {
        Ok((rest, data)) => {
            match String::from_utf8(data) {
                Ok(data) => Ok((rest, data)),
                // TODO Figure out how to pass the error back
                Err(error) => {
                    error!("{}", error);
                    Err(nom::Err::Error(error_position!(
                        input,
                        nom::ErrorKind::Custom(0x15E1)
                    )))
                }
            }
        }
        Err(error) => Err(error),
    }
}

fn parse_limited_path_buf(input: &[u8], size: usize) -> nom::IResult<&[u8], PathBuf> {
    let ret = parse_limited(input, size);
    match ret {
        Ok((rest, data)) => Ok((rest, PathBuf::from(OsString::from_vec(data)))),
        Err(error) => Err(error),
    }
}

#[derive(PartialEq, Debug)]
pub struct Header {
    pub name: String,
    pub id: String,
    pub version: String,
    pub pass: u32,
}

impl Header {
    named!(
        pub parse<Self>,
        do_parse!(
            tag!("PLYSYNC1")
                >> name: apply!(parse_limited_string, 8)
                >> id: apply!(parse_limited_string, 8)
                >> version: apply!(parse_limited_string, 8)
                >> pass: le_u32 >> (Header {
                    name,
                    id,
                    version,
                    pass,
                })
        )
    );
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
    named!(
        pub parse<Self>,
        do_parse!(
            entry_type: alt!(
                          value!(EntryType::End, tag!("e")) |
                          value!(EntryType::File, tag!("f")) |
                          value!(EntryType::Directory, tag!("d"))
            ) >>
            tag!("\0\0\0") >>
                size: le_u32 >>
                mtime: le_u64 >>
                path: apply!(parse_limited_path_buf, 256) >>
                (Entry {
                    entry_type,
                    size,
                    mtime,
                    path,
                })
        )
    );
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
    fn entry_parsing_nom() {
        use super::*;
        let mut data = String::from("d").into_bytes();
        data.push(0x00);
        data.push(0x00);
        data.push(0x00);
        // size
        data.push(0xAB);
        data.push(0xCD);
        data.push(0xEF);
        data.push(0x05);
        // mtime
        data.push(0xCD);
        data.push(0xAB);
        data.push(0xEF);
        data.push(0x05);
        data.push(0x00);
        data.push(0x00);
        data.push(0x00);
        data.push(0x00);
        // path
        data.extend_from_slice("/test".as_bytes());
        for _ in 0..251 {
            data.push(0);
        }
        println!("{}", data.len());
        assert_eq!(
            Entry::parse(&data).unwrap().1,
            Entry {
                entry_type: EntryType::Directory,
                size: 0x05EFCDAB,
                mtime: 0x05EFABCD,
                path: "/test".to_string().into(),
            }
        );
    }
    #[test]
    fn header_parsing_nom() {
        use super::*;
        let mut data = String::from("PLYSYNC13ds\0\0\0\0\0SlimBlue0.0.0\0\0\0").into_bytes();
        data.push(0xAB);
        data.push(0xCD);
        data.push(0xEF);
        data.push(0x05);
        assert_eq!(
            Header::parse(&data).unwrap().1,
            Header {
                name: "3ds".to_string(),
                id: "SlimBlue".to_string(),
                version: "0.0.0".to_string(),
                pass: 0x05EFCDAB,
            }
        );
    }
}
