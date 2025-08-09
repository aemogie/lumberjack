(use-modules ((guix profiles) #:select (packages->manifest))
	     ((gnu packages commencement) #:select (gcc-toolchain))
	     ((gnu packages aspell) #:select (aspell)))

(packages->manifest (list gcc-toolchain))
