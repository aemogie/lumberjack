use crate::gl;

macro_rules! shader_enum {
    ($($name:ident = $val:path),* $(,)?) => {
        mod seal {
            pub trait Seal {}
        }
        // convert the enum to a type you can work with at compile-time
        pub trait Kind: seal::Seal {
            const VALUE: Value;
            const NAME: &'static str;
        }

        #[repr(u32)]
        pub enum Value {
            $($name = $val,)*
        }
        $(
          pub struct $name;
          impl seal::Seal for $name {}
          impl Kind for $name {
              const VALUE: Value = Value::$name as _;
              const NAME: &'static str = stringify!($name);
          }
        )*
    };
}

shader_enum! {
    Compute = gl::COMPUTE_SHADER,
    Vertex = gl::VERTEX_SHADER,
    TessControl = gl::TESS_CONTROL_SHADER,
    TessEvaluation = gl::TESS_EVALUATION_SHADER,
    Geometry = gl::GEOMETRY_SHADER,
    Fragment = gl::FRAGMENT_SHADER,
}
