use super::{config, crypto, crypto::Encrypted, logs, opt, parse};
use failure::Error;

use byteorder::{LittleEndian, ReadBytesExt};
use failure;
use failure::ResultExt;
use filetime;
use filetime::FileTime;
use murmur3::murmur3_32::MurmurHasher;
use std::collections::HashMap;
use std::ffi;
use std::fs;
use std::hash::Hasher;
use std::io;
use std::io::{BufRead, Write};
use std::mem::drop;
use std::net::{TcpListener, TcpStream};
use std::os::unix::ffi::OsStrExt;
use std::path;
use std::result::Result;
use std::sync;
use std::thread;
use std::time::Duration;
use toml;

pub fn run(opt: opt::Opt) -> Result<(), Error> {
    let text = fs::read(opt.config).context("Reading config file")?;
    let config: config::Config = toml::from_slice(&text).context("Parsing config")?;
    let map = sync::Arc::new(config.devices);
    let port = if let Some(port) = config.port {
        port
    } else {
        9483
    };
    let listener = TcpListener::bind(("0.0.0.0", port))?;
    for stream in listener.incoming() {
        let map = sync::Arc::clone(&map);
        thread::spawn(move || {
            let stream = stream.unwrap();
            sync_process(stream, &map)
                .map_err(|e| logs::report(&e))
                .ok();
        });
        // Just some very basic bruteforce protection
        // This should make bruteforcing the key impossible
        thread::sleep(Duration::from_secs(1));
    }
    Ok(())
}

fn sync_process(stream: TcpStream, devices: &HashMap<String, config::Device>) -> Result<(), Error> {
    let (mut enc_comm, id) = crypto::Encrypted::begin_encrypted_communication(stream, devices)
        .context("Couldn't begin encrypted communication")?;
    let header_data = enc_comm
        .receive(parse::HEADER_SIZE as usize)
        .context("Reading Header")?;
    // Currently not needed
    let _header = parse::Header::parse(&header_data)
        .map_err(|err| format_err!("Failed to parse header: {}", err))?
        .1;
    let base_dir = if let Some(device) = devices.get(&id) {
        path::PathBuf::from(&device.dir)
    } else {
        bail!(
            "Device {} isn't registered, even though we were able to initiate communication",
            &id
        );
    };
    let (dir_map, time_check_files, mut missing_files) =
        receive_entries(&mut enc_comm, &base_dir).context("Receiving entries")?;
    for entry in time_check_files {
        if !check_file_checksum(&mut enc_comm, &base_dir, &entry)
            .context("Checking the file checksum")?
        {
            missing_files.push(entry);
        } else {
            let mut full_path = get_full_path(&base_dir, &entry.path)?;
            set_file_mtime(&full_path, entry.mtime as i64)
                .context("Adjusting mtime for checksummed file")?;
        }
    }

    for entry in missing_files {
        download_file(&mut enc_comm, &base_dir, &entry)
            .context(format!("Downloading File {}", entry.path.display()))?;
    }
    let mut header = [0; parse::REQUEST_SIZE as usize];
    header[0] = b'e';
    enc_comm
        .send(&header)
        .context("Writing final download header")?;
    clean_tree_up(&base_dir, dir_map).context("Cleaning tree up")?;
    Ok(())
}

fn receive_entries(
    enc_comm: &mut Encrypted,
    base_dir: &path::Path,
) -> Result<
    (
        HashMap<ffi::OsString, MapEntry>,
        Vec<parse::Entry>,
        Vec<parse::Entry>,
    ),
    Error,
> {
    let mut dir_map = HashMap::new();
    let mut time_check_files = Vec::new();
    let mut missing_files = Vec::new();
    let mut entry_buffer;
    let mut entry;

    entry_buffer = enc_comm
        .receive(parse::ENTRY_SIZE as usize)
        .context("Reading entry")?;
    entry = parse::Entry::parse(&entry_buffer)
        .map_err(|err| format_err!("Failed to parse entry: {}", err))?
        .1;
    while entry.entry_type != parse::EntryType::End {
        let path_buf = path::PathBuf::from(&entry.path);
        let mut path = path_buf.iter();
        let next = path.next();
        if let Some(mut next) = next {
            if next == "/" {
                next = if let Some(next) = path.next() {
                    next
                } else {
                    bail!("Path only contains leading slash");
                }
            }
            add_dir_entry(next, path, &mut dir_map, &entry)
                .context(format!("Adding dir entry for {}", entry.path.display()))?;
        } else {
            bail!("Path is empty");
        }

        if entry.entry_type == parse::EntryType::File {
            match check_existing(&base_dir, &entry).context("Checking file status on disk")? {
                FileState::Matches => (),
                FileState::WrongTime => time_check_files.push(entry),
                FileState::Missing => missing_files.push(entry),
            }
        }
        entry_buffer = enc_comm
            .receive(parse::ENTRY_SIZE as usize)
            .context("Reading entry")?;
        entry = parse::Entry::parse(&entry_buffer)
            .map_err(|err| format_err!("Failed to parse entry: {}", err))?
            .1;
    }
    Ok((dir_map, time_check_files, missing_files))
}

#[derive(Debug)]
enum MapEntry {
    File,
    Dir(HashMap<ffi::OsString, MapEntry>),
}

fn add_dir_entry(
    component: &ffi::OsStr,
    mut path: path::Iter,
    map: &mut HashMap<ffi::OsString, MapEntry>,
    entry: &parse::Entry,
) -> Result<(), failure::Error> {
    if component == ".." || component == "." {
        bail!("Invalid path entry in file {}", &entry.path.display());
    }
    let next = path.next();
    match next {
        None => if entry.entry_type == parse::EntryType::File {
            if map.contains_key(component) {
                // WTF, we got that file twice?
                bail!("File is already in map");
            } else {
                map.insert(component.to_os_string(), MapEntry::File);
            }
        } else if !map.contains_key(component) {
            map.insert(component.to_os_string(), MapEntry::Dir(HashMap::new()));
        },
        Some(next) => {
            if !map.contains_key(component) {
                map.insert(component.to_os_string(), MapEntry::Dir(HashMap::new()));
            }
            if let Some(dir_entry) = map.get_mut(component) {
                if let MapEntry::Dir(map) = dir_entry {
                    add_dir_entry(next, path, map, entry)?;
                } else {
                    bail!(
                        "There's a file & a directory with the same path named {:?}",
                        component
                    );
                }
            } else {
                bail!("Unexpectedly, there's no component entry, even though we checked");
            }
        }
    }
    Ok(())
}

enum FileState {
    Matches,
    WrongTime,
    Missing,
}

fn check_existing(base: &path::Path, entry: &parse::Entry) -> Result<FileState, failure::Error> {
    let full_path = get_full_path(base, &entry.path)?;
    if let Ok(attr) = fs::metadata(&full_path) {
        if attr.len() == u64::from(entry.size) {
            let mtime = FileTime::from_last_modification_time(&attr).unix_seconds() as u64;
            if mtime == entry.mtime {
                Ok(FileState::Matches)
            } else {
                Ok(FileState::WrongTime)
            }
        } else {
            Ok(FileState::Missing)
        }
    } else {
        Ok(FileState::Missing)
    }
}

fn check_file_checksum(
    enc_comm: &mut Encrypted,
    base: &path::Path,
    entry: &parse::Entry,
) -> Result<bool, failure::Error> {
    let mut header = [0; parse::REQUEST_SIZE as usize];
    header[0] = b'h';
    for (num, entry) in entry.path.as_os_str().as_bytes().iter().enumerate() {
        if num == 256 {
            bail!("File path too long, should have been checked");
        }
        header[num + 1] = *entry;
    }
    enc_comm.send(&header).context("Sending checksum request")?;

    let full_path = get_full_path(base, &entry.path)?;

    let file = fs::File::open(full_path).context("Couldn't open previously checked file")?;
    // Add check for dir
    // Make sure this is a multiple of four, otherwise the hash doesn't work
    let mut file = io::BufReader::with_capacity(parse::READ_SIZE as usize, file);
    let mut hasher = MurmurHasher::default();

    loop {
        let consumed = {
            let bytes = file.fill_buf()?;
            if bytes.is_empty() {
                break;
            }
            hasher.write(bytes);
            bytes.len()
        };
        file.consume(consumed);
    }
    let hash = hasher.finish() as u32;
    let hash_read_buffer = enc_comm.receive(4).context("Receiving Hash")?;
    // This shouldn't fail
    let received_hash = io::Cursor::new(&hash_read_buffer)
        .read_u32::<LittleEndian>()
        .context("Couldn't parse hash")?;
    Ok(hash == received_hash)
}

fn download_file(
    enc_comm: &mut Encrypted,
    base: &path::Path,
    entry: &parse::Entry,
) -> Result<(), failure::Error> {
    let mut header = [0; parse::REQUEST_SIZE as usize];
    header[0] = b'f';
    for (num, entry) in entry.path.as_os_str().as_bytes().iter().enumerate() {
        if num == 256 {
            bail!("File path too long, should have been checked");
        }
        header[num + 1] = *entry;
    }
    enc_comm.send(&header).context("Sending download header")?;

    let file_size_buffer = enc_comm
        .receive(4)
        .context("Receiving download file size")?;
    // This shouldn't fail
    let file_size = io::Cursor::new(&file_size_buffer)
        .read_u32::<LittleEndian>()
        .context("Parsing download file size")?;
    if file_size != entry.size {
        bail!(
            "Previously received size doesn't match, now: {}, prev: {} for file {}",
            file_size,
            entry.size,
            entry.path.display()
        );
    }
    let full_path = get_full_path(base, &entry.path).context("Getting full path")?;
    let dir = full_path
        .parent()
        .ok_or_else(|| format_err!("Couldn't get parent for {}", full_path.display()))?;
    fs::create_dir_all(dir).context(format!("Creating dirs for {}", full_path.display()))?;
    // If there was a dir with this name, remove it now
    // If it fails, it's probably because nothing was there, otherwise
    // File::create will also fail
    fs::remove_dir_all(&full_path).ok();
    let mut file = fs::File::create(&full_path).context("Opening/Creating download file")?;

    let mut size_left = file_size;
    let mut hasher = MurmurHasher::default();

    while size_left != 0 {
        let read_size = if size_left >= parse::READ_SIZE {
            parse::READ_SIZE
        } else {
            size_left
        };

        let read_buffer = enc_comm
            .receive(read_size as usize)
            .context("Receiving download file fragment")?;
        hasher.write(&read_buffer);
        file.write(&read_buffer)
            .context("Writing download file fragment to file")?;
        size_left -= read_size;
    }
    let hash = hasher.finish() as u32;
    let hash_buffer = enc_comm.receive(4).context("Reading download hash")?;
    // This shouldn't fail
    let received_hash = io::Cursor::new(&hash_buffer)
        .read_u32::<LittleEndian>()
        .context("Parsing download hash")?;

    // Close the file so we can adjust mtime
    drop(file);
    if hash != received_hash {
        bail!(
            "Download ({:x}) doesn't match hash {:x}",
            hash,
            received_hash
        );
    }

    set_file_mtime(&full_path, entry.mtime as i64)
        .context("Adjusting mtime for downloaded file")?;
    Ok(())
}

fn set_file_mtime(path: &path::Path, mtime: i64) -> Result<(), failure::Error> {
    let attr = fs::metadata(path).context("Opening file for mtime adjustmets")?;
    let mtime = FileTime::from_unix_time(mtime as i64, 0);
    let atime = FileTime::from_last_access_time(&attr);
    filetime::set_file_times(path, atime, mtime).context("Setting mtime")?;
    Ok(())
}

fn get_full_path(
    base: &path::Path,
    mut path: &path::Path,
) -> Result<path::PathBuf, failure::Error> {
    let mut full_path = path::PathBuf::from(base);
    if let Ok(npath) = path.strip_prefix("/") {
        path = npath;
    }
    full_path.push(path);
    if !full_path.starts_with(base) {
        bail!(
            "Resulting path {} isn't within base directory",
            full_path.display()
        );
    }
    Ok(full_path)
}

fn clean_tree_up(
    path: &path::Path,
    mut map: HashMap<ffi::OsString, MapEntry>,
) -> Result<(), failure::Error> {
    for entry in fs::read_dir(path)? {
        let entry = entry.context("Reading dir entry")?;
        if let Some(dir_entry) = map.remove(&entry.file_name()) {
            let metadata = &entry.file_type().context("Getting entry file type")?;
            match dir_entry {
                MapEntry::File => {
                    ensure!(!metadata.is_dir(), "Dir at the position of expected file")
                }
                MapEntry::Dir(map) => {
                    ensure!(metadata.is_dir(), "File at the position of expected dir");
                    let mut new_path = path::PathBuf::from(path);
                    new_path.push(&entry.file_name());
                    clean_tree_up(&new_path, map)?;
                }
            }
        } else {
            fs::remove_dir_all(entry.path()).context("Removing unneded dir")?;
        }
    }
    create_missing_dirs(path, map).context("Creating missing dirs")?;
    Ok(())
}

fn create_missing_dirs(
    path: &path::Path,
    mut map: HashMap<ffi::OsString, MapEntry>,
) -> Result<(), failure::Error> {
    for (name, entry) in map.drain() {
        match entry {
            MapEntry::File => bail!("File in map is missing on-disk"),
            MapEntry::Dir(map) => {
                let mut new_path = path::PathBuf::from(path);
                new_path.push(name);
                fs::create_dir(&new_path).context("Creating dir")?;
                create_missing_dirs(&new_path, map)?;
            }
        }
    }
    Ok(())
}
