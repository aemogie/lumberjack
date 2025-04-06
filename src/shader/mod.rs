pub mod kind;

use std::{
    error::Error,
    ffi::CString,
    fmt::{self, Debug, Display},
    io,
    marker::PhantomData,
    path::Path,
    result::Result,
};

use crate::gl::{
    self,
    types::{GLchar, GLint, GLuint},
};

pub struct ShaderSource {
    text: String,
}
pub struct ShaderError(io::Error);
impl From<io::Error> for ShaderError {
    fn from(err: io::Error) -> Self {
        Self(err)
    }
}
impl ShaderSource {
    pub fn load(path: &Path) -> Result<Self, ShaderError> {
        Ok(Self {
            text: std::fs::read_to_string(path)?,
        })
    }

    pub fn upload<Kind: kind::Kind>(
        self,
        object: ShaderObject<Kind>,
    ) -> Result<UploadedShader<Kind>, ShaderUploadError<Kind>> {
        UploadedShader::upload(object, self)
    }
}

pub struct ShaderCreationError<Kind: kind::Kind>(PhantomData<Kind>);
impl<Kind: kind::Kind> Display for ShaderCreationError<Kind> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "backend doesn't support shader type: {}", Kind::NAME)
    }
}
impl<Kind: kind::Kind> Debug for ShaderCreationError<Kind> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self)
    }
}
impl<Kind: kind::Kind> Error for ShaderCreationError<Kind> {}

pub struct ShaderObject<Kind: kind::Kind> {
    _kind: PhantomData<Kind>,
    id: GLuint,
}
impl<Kind: kind::Kind> ShaderObject<Kind> {
    pub fn create() -> Result<Self, ShaderCreationError<Kind>> {
        debug_assert!(
            unsafe { gl::GetError() } == gl::NO_ERROR,
            "dirty error queue"
        );
        match unsafe { gl::CreateShader(Kind::VALUE as _) } {
            0 => match unsafe { gl::GetError() } {
                gl::INVALID_ENUM => Err(ShaderCreationError(PhantomData)),
                gl::NO_ERROR => unreachable!("no errors reported"),
                _ => todo!("OpenGL: glGetShaderInfoLog, glGetShaderiv"),
            },
            id => Ok(Self {
                _kind: PhantomData,
                id,
            }),
        }
    }
    pub fn upload(
        self,
        source: ShaderSource,
    ) -> Result<UploadedShader<Kind>, ShaderUploadError<Kind>> {
        UploadedShader::upload(self, source)
    }
}

pub struct UploadedShader<Kind: kind::Kind> {
    object: ShaderObject<Kind>,
}
pub struct ShaderUploadError<Kind: kind::Kind>(PhantomData<Kind>);
impl<Kind: kind::Kind> Display for ShaderUploadError<Kind> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "failed while uploading shader type: {}", Kind::NAME)
    }
}
impl<Kind: kind::Kind> Debug for ShaderUploadError<Kind> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self)
    }
}
impl<Kind: kind::Kind> Error for ShaderUploadError<Kind> {}
impl<Kind: kind::Kind> UploadedShader<Kind> {
    pub fn upload(
        object: ShaderObject<Kind>,
        source: ShaderSource,
    ) -> Result<Self, ShaderUploadError<Kind>> {
        debug_assert!(
            unsafe { gl::GetError() } == gl::NO_ERROR,
            "dirty error queue"
        );
        let gl_len = source.text.len() as GLint;
        let gl_len_arr = [gl_len].as_ptr();
        let c_string = CString::new(source.text).unwrap();
        let gl_string = c_string.as_ptr() as *const GLchar;
        let gl_string_arr = [gl_string].as_ptr();
        unsafe { gl::ShaderSource(object.id as _, 1, gl_string_arr, gl_len_arr) };
        match unsafe { gl::GetError() } {
            gl::NO_ERROR => Ok(Self { object }),
            gl::INVALID_VALUE => {
                // opengl is stupd and uses the same error enum for both
                let mut params: GLint = -1;
                unsafe {
                    gl::GetShaderiv(object.id, gl::SHADER_SOURCE_LENGTH, &mut params as *mut _);
                };
                if params == 0 {
                    Err(ShaderUploadError(PhantomData))
                } else {
                    panic!("ShaderObject was garbage")
                }
            }
            gl::INVALID_OPERATION => panic!("ShaderObject did not point to a shader"),
            _ => todo!("OpenGL: glGetShaderInfoLog, glGetShaderiv"),
        }
    }
}
