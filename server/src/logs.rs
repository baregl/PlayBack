use failure::Error;
use log::SetLoggerError;
use simplelog::*;

pub fn init(verbosity: u8) -> Result<(), SetLoggerError> {
    SimpleLogger::init(getlvl(verbosity), Config::default())
}

pub fn report(error: &Error) {
    error!("An error occured while: {}", error);
    for e in error.iter_chain().skip(1) {
        error!("Caused by: {}", e);
    }
}
/// Sets the verbosity level of messages that will be displayed
pub fn getlvl(verbosity: u8) -> LevelFilter {
    match verbosity {
        0 => LevelFilter::Error,
        1 => LevelFilter::Warn,
        2 => LevelFilter::Info,
        3 => LevelFilter::Debug,
        _ => LevelFilter::Trace,
    }
}
