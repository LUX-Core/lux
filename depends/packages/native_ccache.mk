package=native_ccache
$(package)_version=3.4.1
$(package)_download_path=https://samba.org/ftp/ccache
$(package)_file_name=ccache-$($(package)_version).tar.bz2
$(package)_sha256_hash=ca5a01fb4868cdb5176c77b8b4a390be7929a6064be80741217e0686f03f8389

define $(package)_set_vars
$(package)_config_opts=
endef

define $(package)_preprocess_cmds
  sed -i '57,60d' Makefile.in; \
  sed -i '57izlib_sources = src/zlib/adler32.c src/zlib/crc32.c src/zlib/deflate.c src/zlib/gzclose.c src/zlib/gzlib.c src/zlib/gzread.c src/zlib/gzwrite.c src/zlib/inffast.c src/zlib/inflate.c src/zlib/inftrees.c src/zlib/trees.c src/zlib/zutil.c' Makefile.in; \
  sed "s/-Werror/-Werror -Wimplicit-fallthrough=0/" dev.mk.in;
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf lib include
endef
