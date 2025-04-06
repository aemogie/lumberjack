use std::{fs, path};

use gl_generator::{Api, Fallbacks, GlobalGenerator, Profile, Registry};

const PATH: &'static str = "src/gl.rs";

fn main() {
    let path = path::Path::new(PATH);
    fs::create_dir_all(path.parent().unwrap()).unwrap();
    let mut file = fs::File::create(path).unwrap();

    Registry::new(Api::Gl, (4, 6), Profile::Core, Fallbacks::All, [])
        .write_bindings(GlobalGenerator, &mut file)
        .unwrap();
}
