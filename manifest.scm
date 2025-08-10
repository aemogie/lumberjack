(use-modules ((guix profiles) #:select (packages->manifest))
	     ((gnu packages llvm) #:select (clang-toolchain-20))
	     ((gnu packages freedesktop) #:select (wayland))
	     ((gnu packages man) #:select (man-db man-pages)))

(packages->manifest (list man-db man-pages
			  clang-toolchain-20
			  wayland (list wayland "doc")))
