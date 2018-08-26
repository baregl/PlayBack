use std::path::PathBuf;
/// A webapp to collect icons for psp games on the vita
#[derive(StructOpt, Debug)]
#[structopt(name = "plybcksrv")]
pub struct Opt {
    /// Verbosity
    #[structopt(short = "v", parse(from_occurrences))]
    pub verbosity: u8,
    /// Config file
    #[structopt(short = "c", default_value = "/etc/plybck.toml", parse(from_os_str))]
    pub config: PathBuf,
}
