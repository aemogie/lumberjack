(use-modules ((guix profiles) #:select (packages->manifest))
	     ((gnu packages commencement) #:select (gcc-toolchain))
	     ((gnu packages man) #:select (man-db man-pages-posix)))

(packages->manifest (list gcc-toolchain
			  man-db man-pages-posix))
