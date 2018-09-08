extern crate plybcksrv;
extern crate structopt;

use plybcksrv::{logs, opt, run};
use std::process::exit;
use structopt::StructOpt;
fn main() {
    let opt = opt::Opt::from_args();
    // Default verbosity warn
    logs::init(opt.verbosity)
        .map_err(|e| {
            println!("{}", e);
            e
        })
        .expect("Initializing Logger");
    match run::run(opt) {
        Ok(()) => exit(0),
        Err(e) => {
            logs::report(e);
            exit(1);
        }
    }
}
