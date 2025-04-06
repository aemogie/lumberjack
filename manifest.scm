(use-modules ((gnu packages rust) #:select (rust rust-analyzer))
	     ((gnu packages rust-apps) #:select (rust-cargo)))

(packages->manifest (list (list rust "out")   ;; rustc
			  (list rust "cargo") ;; cargo
			  ;; rust-analyzer has RUST_SRC_PATH set yet still complains
			  (list rust "tools") ;; rustfmt, rust-analyzer
			  (list rust "rust-src"))) ;; rust-src (unused i think)
