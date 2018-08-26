use std::collections::HashMap;
#[derive(Deserialize, Debug)]
pub struct Config {
    pub port: Option<u16>,
    pub devices: HashMap<String, Device>,
}

#[derive(Deserialize, Debug)]
pub struct Device {
    pub pass: String,
    pub dir: String,
}
