DIST_SUBDIRS = secp256k1
AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS)
AM_CXXFLAGS = $(HARDENED_CXXFLAGS) $(ERROR_CXXFLAGS) -DETH_FATDB=1
AM_CPPFLAGS = $(HARDENED_CPPFLAGS) -DLUX_BUILD

if EMBEDDED_LEVELDB
LEVELDB_CPPFLAGS += -I$(srcdir)/leveldb/include
LEVELDB_CPPFLAGS += -I$(srcdir)/leveldb/helpers/memenv
LIBLEVELDB += $(builddir)/leveldb/libleveldb.a
LIBMEMENV  += $(builddir)/leveldb/libmemenv.a

# NOTE: This dependency is not strictly necessary, but without it make may try to build both in parallel, which breaks the LevelDB build system in a race
$(LIBLEVELDB): $(LIBMEMENV)

$(LIBLEVELDB) $(LIBMEMENV):
	@echo "Building LevelDB ..." && $(MAKE) -C $(@D) $(@F) CXX="$(CXX)" \
	  CC="$(CC)" PLATFORM=$(TARGET_OS) AR="$(AR)" $(LEVELDB_TARGET_FLAGS) \
          OPT="$(CXXFLAGS) $(CPPFLAGS)"
endif

BITCOIN_CONFIG_INCLUDES=-I$(builddir)/config
BITCOIN_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS)

BITCOIN_INCLUDES += -I$(srcdir)/secp256k1/include

BITCOIN_INCLUDES += -I$(srcdir)/cpp-ethereum
BITCOIN_INCLUDES += -I$(srcdir)/cpp-ethereum/utils

LIBBITCOIN_SERVER=libbitcoin_server.a
LIBBITCOIN_WALLET=libbitcoin_wallet.a
LIBBITCOIN_COMMON=libbitcoin_common.a
LIBBITCOIN_CLI=libbitcoin_cli.a
LIBBITCOIN_UTIL=libbitcoin_util.a
LIBBITCOIN_CRYPTO=crypto/libbitcoin_crypto.a
LIBBITCOIN_UNIVALUE=univalue/libbitcoin_univalue.a
LIBBITCOINQT=qt/libbitcoinqt.a
LIBSECP256K1=secp256k1/libsecp256k1.la
LIBCRYPTOPP=cryptopp/libcryptopp.a

$(LIBSECP256K1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES = \
  crypto/libbitcoin_crypto.a \
  libbitcoin_util.a \
  libbitcoin_common.a \
  univalue/libbitcoin_univalue.a \
  libbitcoin_server.a \
  libbitcoin_cli.a
if ENABLE_WALLET
BITCOIN_INCLUDES += $(BDB_CPPFLAGS)
EXTRA_LIBRARIES += libbitcoin_wallet.a
endif

if ENABLE_ZMQ
EXTRA_LIBRARIES += libbitcoin_zmq.a
endif

if BUILD_BITCOIN_LIBS
lib_LTLIBRARIES = libbitcoinconsensus.la
LIBBITCOIN_CONSENSUS=libbitcoinconsensus.la
else
LIBBITCOIN_CONSENSUS=
endif

bin_PROGRAMS =
TESTS =

if BUILD_BITCOIND
  bin_PROGRAMS += luxd
endif

if BUILD_BITCOIN_UTILS
  bin_PROGRAMS += lux-cli lux-tx
endif

.PHONY: FORCE
# lux core #
BITCOIN_CORE_H = \
  activemasternode.h \
  addrman.h \
  alert.h \
  allocators.h \
  amount.h \
  base58.h \
  bip38.h \
  bloom.h \
  chain.h \
  chainparams.h \
  chainparamsbase.h \
  chainparamsseeds.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coincontrol.h \
  coins.h \
  compat.h \
  compressor.h \
  primitives/block.h \
  primitives/transaction.h \
  core_io.h \
  crypter.h \
  darksend.h \
  db.h \
  eccryptoverify.h \
  ecwrapper.h \
  hash.h \
  init.h \
  stake.h \
  instantx.h \
  key.h \
  keystore.h \
  leveldbwrapper.h \
  limitedmap.h \
  main.h \
  masternode.h \
  masternodeconfig.h \
  merkleblock.h \
  miner.h \
  mruset.h \
  netbase.h \
  net.h \
  noui.h \
  pow.h \
  protocol.h \
  pubkey.h \
  random.h \
  rpcclient.h \
  rpcprotocol.h \
  rpcserver.h \
  script/interpreter.h \
  script/script.h \
  script/sigcache.h \
  script/sign.h \
  script/standard.h \
  script/script_error.h \
  serialize.h \
  spork.h \
  streams.h \
  sync.h \
  threadsafety.h \
  timedata.h \
  tinyformat.h \
  txdb.h \
  txmempool.h \
  ui_interface.h \
  uint256.h \
  undo.h \
  util.h \
  utilstrencodings.h \
  utilmoneystr.h \
  utiltime.h \
  validationinterface.h \
  version.h \
  wallet.h \
  wallet_ismine.h \
  walletdb.h \
  zmq/zmqabstractnotifier.h \
  zmq/zmqconfig.h \
  zmq/zmqnotificationinterface.h \
  zmq/zmqpublishnotifier.h

JSON_H = \
  json/json_spirit.h \
  json/json_spirit_error_position.h \
  json/json_spirit_reader.h \
  json/json_spirit_reader_template.h \
  json/json_spirit_stream_reader.h \
  json/json_spirit_utils.h \
  json/json_spirit_value.h \
  json/json_spirit_writer.h \
  json/json_spirit_writer_template.h

obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh $(abs_top_builddir)/src/obj/build.h \
	  $(abs_top_srcdir)
libbitcoin_util_a-clientversion.$(OBJEXT): obj/build.h

# server: shared between luxd and lux-qt
libbitcoin_server_a_CPPFLAGS = $(BITCOIN_INCLUDES) $(MINIUPNPC_CPPFLAGS)
libbitcoin_server_a_SOURCES = \
  addrman.cpp \
  alert.cpp \
  bloom.cpp \
  chain.cpp \
  checkpoints.cpp \
  init.cpp \
  leveldbwrapper.cpp \
  main.cpp \
  merkleblock.cpp \
  miner.cpp \
  net.cpp \
  noui.cpp \
  pow.cpp \
  rest.cpp \
  rpcblockchain.cpp \
  rpcdarksend.cpp \
  rpcmining.cpp \
  rpcmisc.cpp \
  rpcnet.cpp \
  rpcrawtransaction.cpp \
  rpcserver.cpp \
  script/sigcache.cpp \
  timedata.cpp \
  txdb.cpp \
  txmempool.cpp \
  validationinterface.cpp \
  $(JSON_H) \
  $(BITCOIN_CORE_H)

if ENABLE_ZMQ
LIBBITCOIN_ZMQ=libbitcoin_zmq.a

libbitcoin_zmq_a_CPPFLAGS = $(BITCOIN_INCLUDES) $(ZMQ_CFLAGS)
libbitcoin_zmq_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libbitcoin_zmq_a_SOURCES = \
  zmq/zmqabstractnotifier.cpp \
  zmq/zmqnotificationinterface.cpp \
  zmq/zmqpublishnotifier.cpp
endif

# wallet: shared between luxd and lux-qt, but only linked
# when wallet enabled

libbitcoin_wallet_a_CFLAGS = -fPIC
libbitcoin_wallet_a_CPPFLAGS = $(BITCOIN_INCLUDES)
libbitcoin_wallet_a_SOURCES = \
  activemasternode.cpp \
  bip38.cpp \
  darksend.cpp \
  db.cpp \
  crypter.cpp \
  instantx.cpp \
  masternode.cpp \
  masternodeconfig.cpp \
  rpcdump.cpp \
  rpcwallet.cpp \
  stake.cpp \
  wallet.cpp \
  wallet_ismine.cpp \
  walletdb.cpp \
  $(BITCOIN_CORE_H)

# crypto primitives library
crypto_libbitcoin_crypto_a_CFLAGS = -fPIC
crypto_libbitcoin_crypto_a_CPPFLAGS = $(BITCOIN_CONFIG_INCLUDES)
crypto_libbitcoin_crypto_a_SOURCES = \
  crypto/sha1.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  crypto/hmac_sha256.cpp \
  crypto/rfc6979_hmac_sha256.cpp \
  crypto/hmac_sha512.cpp \
  crypto/scrypt.cpp \
  crypto/ripemd160.cpp \
  crypto/aes_helper.c \
  crypto/echo.c \
  crypto/cubehash.c \
  crypto/jh.c \
  crypto/skein.c \
  crypto/gost.c \
  crypto/fugue.c \
  crypto/common.h \
  crypto/sha256.h \
  crypto/sha512.h \
  crypto/hmac_sha256.h \
  crypto/rfc6979_hmac_sha256.h \
  crypto/hmac_sha512.h \
  crypto/scrypt.h \
  crypto/sha1.h \
  crypto/ripemd160.h \
  crypto/sph_types.h \
  crypto/sph_jh.h \
  crypto/sph_skein.h \
  crypto/sph_fugue.h \
  crypto/sph_gost.h \
  crypto/sph_cubehash.h \
  crypto/sph_echo.h \
  cpp-ethereum/utils/libscrypt/b64.c \
  cpp-ethereum/utils/libscrypt/b64.h \
  cpp-ethereum/utils/libscrypt/crypto-mcf.c \
  cpp-ethereum/utils/libscrypt/crypto_scrypt-check.c \
  cpp-ethereum/utils/libscrypt/crypto_scrypt-hash.c \
  cpp-ethereum/utils/libscrypt/crypto_scrypt-hexconvert.c \
  cpp-ethereum/utils/libscrypt/crypto_scrypt-hexconvert.h \
  cpp-ethereum/utils/libscrypt/crypto_scrypt-nosse.c \
  cpp-ethereum/utils/libscrypt/crypto-scrypt-saltgen.c \
  cpp-ethereum/utils/libscrypt/libscrypt.h \
  cpp-ethereum/utils/libscrypt/sha256.c \
  cpp-ethereum/utils/libscrypt/sha256.h \
  cpp-ethereum/utils/libscrypt/slowequals.c \
  cpp-ethereum/utils/libscrypt/slowequals.h \
  cpp-ethereum/utils/libscrypt/sysendian.h

# univalue JSON library
univalue_libbitcoin_univalue_a_SOURCES = \
  univalue/univalue.cpp \
  univalue/univalue_read.cpp \
  univalue/univalue_write.cpp \
  univalue/univalue_escapes.h \
  univalue/univalue.h

# common: shared between luxd, and lux-qt and non-server tools
libbitcoin_common_a_CPPFLAGS = $(BITCOIN_INCLUDES)
libbitcoin_common_a_SOURCES = \
  allocators.cpp \
  amount.cpp \
  base58.cpp \
  bip38.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  primitives/block.cpp \
  primitives/transaction.cpp \
  core_read.cpp \
  core_write.cpp \
  eccryptoverify.cpp \
  ecwrapper.cpp \
  hash.cpp \
  key.cpp \
  keystore.cpp \
  netbase.cpp \
  protocol.cpp \
  pubkey.cpp \
  script/interpreter.cpp \
  script/script.cpp \
  script/sign.cpp \
  script/standard.cpp \
  script/script_error.cpp \
  spork.cpp \
  cpp-ethereum/libdevcore/Base64.cpp \
  cpp-ethereum/libdevcore/Base64.h \
  cpp-ethereum/libdevcore/Common.cpp \
  cpp-ethereum/libdevcore/Common.h \
  cpp-ethereum/libdevcore/CommonData.cpp \
  cpp-ethereum/libdevcore/CommonData.h \
  cpp-ethereum/libdevcore/CommonIO.cpp \
  cpp-ethereum/libdevcore/CommonIO.h \
  cpp-ethereum/libdevcore/CommonJS.cpp \
  cpp-ethereum/libdevcore/CommonJS.h \
  cpp-ethereum/libdevcore/FileSystem.cpp \
  cpp-ethereum/libdevcore/FileSystem.h \
  cpp-ethereum/libdevcore/FixedHash.cpp \
  cpp-ethereum/libdevcore/FixedHash.h \
  cpp-ethereum/libdevcore/Guards.cpp \
  cpp-ethereum/libdevcore/Guards.h \
  cpp-ethereum/libdevcore/Hash.cpp \
  cpp-ethereum/libdevcore/Hash.h \
  cpp-ethereum/libdevcore/Log.cpp \
  cpp-ethereum/libdevcore/Log.h \
  cpp-ethereum/libdevcore/MemoryDB.cpp \
  cpp-ethereum/libdevcore/MemoryDB.h \
  cpp-ethereum/libdevcore/OverlayDB.cpp \
  cpp-ethereum/libdevcore/OverlayDB.h \
  cpp-ethereum/libdevcore/RLP.cpp \
  cpp-ethereum/libdevcore/RLP.h \
  cpp-ethereum/libdevcore/SHA3.cpp \
  cpp-ethereum/libdevcore/SHA3.h \
  cpp-ethereum/libdevcore/TransientDirectory.cpp \
  cpp-ethereum/libdevcore/TransientDirectory.h \
  cpp-ethereum/libdevcore/TrieCommon.cpp \
  cpp-ethereum/libdevcore/TrieCommon.h \
  cpp-ethereum/libdevcore/Worker.cpp \
  cpp-ethereum/libdevcore/Worker.h \
  cpp-ethereum/libevm/ExtVMFace.cpp \
  cpp-ethereum/libevm/ExtVMFace.h \
  cpp-ethereum/libevm/VM.cpp \
  cpp-ethereum/libevm/VM.h \
  cpp-ethereum/libevm/VMOpt.cpp \
  cpp-ethereum/libevm/VMCalls.cpp \
  cpp-ethereum/libevm/VMFactory.cpp \
  cpp-ethereum/libevm/VMFactory.h \
  cpp-ethereum/libevmcore/Instruction.cpp \
  cpp-ethereum/libevmcore/Instruction.h \
  cpp-ethereum/libevmcore/Exceptions.h \
  cpp-ethereum/libevmcore/EVMSchedule.h \
  cpp-ethereum/libethereum/Account.cpp \
  cpp-ethereum/libethereum/Defaults.cpp \
  cpp-ethereum/libethereum/GasPricer.cpp \
  cpp-ethereum/libethereum/State.cpp \
  cpp-ethereum/libethcore/ABI.cpp \
  cpp-ethereum/libethcore/ChainOperationParams.cpp \
  cpp-ethereum/libethcore/Common.cpp \
  cpp-ethereum/libethcore/ICAP.cpp \
  cpp-ethereum/libethcore/Precompiled.cpp \
  cpp-ethereum/libdevcore/TrieDB.cpp \
  cpp-ethereum/libdevcrypto/Common.cpp \
  cpp-ethereum/libdevcrypto/CryptoPP.cpp \
  cpp-ethereum/libdevcrypto/AES.cpp \
  cpp-ethereum/libdevcrypto/ECDHE.cpp \
  cpp-ethereum/libethashseal/GenesisInfo.cpp \
  cpp-ethereum/libethereum/ChainParams.cpp \
  cpp-ethereum/libethcore/Transaction.cpp \
  cpp-ethereum/libethereum/Transaction.cpp \
  cpp-ethereum/libethereum/Executive.cpp \
  cpp-ethereum/libethereum/ExtVM.cpp \
  cpp-ethereum/libethereum/Block.cpp \
  cpp-ethereum/libethereum/BlockChain.cpp \
  cpp-ethereum/libethereum/BlockDetails.cpp \
  cpp-ethereum/libethereum/TransactionQueue.cpp \
  cpp-ethereum/libethereum/BlockQueue.cpp \
  cpp-ethereum/libethcore/BlockHeader.cpp \
  cpp-ethereum/libdevcore/RLP.cpp \
  cpp-ethereum/libethereum/TransactionReceipt.cpp \
  cpp-ethereum/libethcore/SealEngine.cpp \
  cpp-ethereum/libdevcore/TrieHash.cpp \
  cpp-ethereum/libethereum/GenesisInfo.cpp \
  cpp-ethereum/libethashseal/Ethash.cpp \
  cpp-ethereum/libethashseal/EthashCPUMiner.cpp \
  cpp-ethereum/libethashseal/EthashAux.cpp \
  cpp-ethereum/libethashseal/EthashProofOfWork.cpp \
  cpp-ethereum/libethereum/Account.h \
  cpp-ethereum/libethereum/Defaults.h \
  cpp-ethereum/libethereum/GasPricer.h \
  cpp-ethereum/libethereum/State.h \
  cpp-ethereum/libethcore/ABI.h \
  cpp-ethereum/libethcore/ChainOperationParams.h \
  cpp-ethereum/libethcore/Common.h \
  cpp-ethereum/libethcore/ICAP.h \
  cpp-ethereum/libethcore/Precompiled.h \
  cpp-ethereum/libdevcore/TrieDB.h \
  cpp-ethereum/libdevcrypto/Common.h \
  cpp-ethereum/libdevcrypto/CryptoPP.h \
  cpp-ethereum/libdevcrypto/AES.h \
  cpp-ethereum/libdevcrypto/ECDHE.h \
  cpp-ethereum/libethashseal/GenesisInfo.h \
  cpp-ethereum/libethereum/ChainParams.h \
  cpp-ethereum/libethcore/Transaction.h \
  cpp-ethereum/libethcore/Exceptions.h \
  cpp-ethereum/libethereum/Transaction.h \
  cpp-ethereum/libethereum/Executive.h \
  cpp-ethereum/libethereum/ExtVM.h \
  cpp-ethereum/libethereum/Block.h \
  cpp-ethereum/libethereum/BlockChain.h \
  cpp-ethereum/libethereum/BlockDetails.h \
  cpp-ethereum/libethereum/TransactionQueue.h \
  cpp-ethereum/libethereum/BlockQueue.h \
  cpp-ethereum/libethcore/BlockHeader.h \
  cpp-ethereum/libdevcore/RLP.h \
  cpp-ethereum/libethereum/TransactionReceipt.h \
  cpp-ethereum/libethcore/SealEngine.h \
  cpp-ethereum/libdevcore/TrieHash.h \
  cpp-ethereum/libethereum/GenesisInfo.h \
  cpp-ethereum/libethashseal/Ethash.h \
  cpp-ethereum/libethashseal/EthashCPUMiner.h \
  cpp-ethereum/libethashseal/EthashAux.h \
  cpp-ethereum/libethashseal/EthashProofOfWork.h \
  cpp-ethereum/libethash/internal.c \
  cpp-ethereum/libethash/io.c \
  cpp-ethereum/libethash/sha3.c \
  cpp-ethereum/libethash/internal.h \
  cpp-ethereum/libethash/io.h \
  cpp-ethereum/libethash/sha3.h \
  cpp-ethereum/libdevcore/vector_ref.h \
  cpp-ethereum/libdevcore/Exceptions.h \
  cpp-ethereum/libdevcore/db.h \
  cpp-ethereum/libdevcore/concurrent_queue.h \
  cpp-ethereum/libdevcore/Terminal.h \
  cpp-ethereum/libethereum/CodeSizeCache.h \
  cpp-ethereum/libethereum/VerifiedBlock.h \
  cpp-ethereum/libdevcore/Assertions.h \
  cpp-ethereum/libdevcore/debugbreak.h \
  cpp-ethereum/libethereum/GenericMiner.h \
  cpp-ethereum/libevm/VMFace.h \
  cpp-ethereum/libethereum/GenericFarm.h \
  cpp-ethereum/libethereum/Interface.h \
  cpp-ethereum/libethcore/CommonJS.h \
  cpp-ethereum/libdevcore/picosha2.h \
  cpp-ethereum/libevm/VMConfig.h \
  cpp-ethereum/libethereum/LogFilter.h \
  cpp-ethereum/libdevcrypto/Exceptions.h \
  cpp-ethereum/libethashseal/genesis/mainNetwork.cpp \
  cpp-ethereum/libethashseal/genesis/eip150Test.cpp \
  cpp-ethereum/libethashseal/genesis/eip158Test.cpp \
  cpp-ethereum/libethashseal/genesis/frontierTest.cpp \
  cpp-ethereum/libethashseal/genesis/homesteadTest.cpp \
  cpp-ethereum/libethashseal/genesis/mainNetworkTest.cpp \
  cpp-ethereum/libethashseal/genesis/metropolisTest.cpp \
  cpp-ethereum/libethashseal/genesis/luxMainNetwork.cpp \
  cpp-ethereum/libethashseal/genesis/luxTestNetwork.cpp \
  cpp-ethereum/libethashseal/genesis/ropsten.cpp \
  cpp-ethereum/libethashseal/genesis/transitionnetTest.cpp \
  cpp-ethereum/libethash/ethash.h \
  cpp-ethereum/libethash/compiler.h \
  cpp-ethereum/libethash/data_sizes.h \
  cpp-ethereum/libethash/fnv.h \
  cpp-ethereum/libethash/mmap.h \
  cpp-ethereum/libethash/endian.h \
  cpp-ethereum/libethash/io.h \
  cpp-ethereum/libethash/ethash.h \
  cpp-ethereum/libethash/internal.h \
  cpp-ethereum/libethash/mmap_win32.c \
  cpp-ethereum/utils/json_spirit/JsonSpiritHeaders.h \
  cpp-ethereum/utils/json_spirit/json_spirit.h \
  cpp-ethereum/utils/json_spirit/json_spirit_error_position.h \
  cpp-ethereum/utils/json_spirit/json_spirit_reader.h \
  cpp-ethereum/utils/json_spirit/json_spirit_reader_template.h \
  cpp-ethereum/utils/json_spirit/json_spirit_stream_reader.h \
  cpp-ethereum/utils/json_spirit/json_spirit_utils.h \
  cpp-ethereum/utils/json_spirit/json_spirit_value.h \
  cpp-ethereum/utils/json_spirit/json_spirit_writer.h \
  cpp-ethereum/utils/json_spirit/json_spirit_writer_template.h \
  $(BITCOIN_CORE_H)

if TARGET_WINDOWS
    libbitcoin_common_a_SOURCES += \
      cpp-ethereum/libethash/io_win32.c \
      cpp-ethereum/libethash/util_win32.c
else
    libbitcoin_common_a_SOURCES += cpp-ethereum/libethash/io_posix.c
endif

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libbitcoin_util_a_CPPFLAGS = $(BITCOIN_INCLUDES)
libbitcoin_util_a_SOURCES = \
  allocators.cpp \
  compat/strnlen.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  chainparamsbase.cpp \
  clientversion.cpp \
  random.cpp \
  rpcprotocol.cpp \
  sync.cpp \
  uint256.cpp \
  util.cpp \
  utilstrencodings.cpp \
  utilmoneystr.cpp \
  utiltime.cpp \
  $(BITCOIN_CORE_H)

if GLIBC_BACK_COMPAT
libbitcoin_util_a_SOURCES += compat/glibc_compat.cpp
libbitcoin_util_a_SOURCES += compat/glibcxx_compat.cpp
endif

# cli: shared between lux-cli and lux-qt
libbitcoin_cli_a_CPPFLAGS = $(BITCOIN_INCLUDES)
libbitcoin_cli_a_SOURCES = \
  rpcclient.cpp \
  $(BITCOIN_CORE_H)

nodist_libbitcoin_util_a_SOURCES = $(srcdir)/obj/build.h
#

# bitcoind binary #
luxd_LDADD = \
  $(LIBBITCOIN_SERVER) \
  $(LIBBITCOIN_COMMON) \
  $(LIBBITCOIN_UNIVALUE) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBLEVELDB) \
  $(LIBMEMENV) \
  $(LIBCRYPTOPP) \
  $(LIBSECP256K1)

if ENABLE_ZMQ
luxd_LDADD += $(LIBBITCOIN_ZMQ) $(ZMQ_LIBS)
endif

if ENABLE_WALLET
luxd_LDADD += libbitcoin_wallet.a
endif
luxd_SOURCES = luxd.cpp
#

if TARGET_WINDOWS
luxd_SOURCES += luxd-res.rc
endif

luxd_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS)
luxd_CPPFLAGS = $(BITCOIN_INCLUDES)
luxd_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)


# lux-cli binary #
lux_cli_LDADD = \
  $(LIBBITCOIN_CLI) \
  $(LIBBITCOIN_UTIL) \
  $(BOOST_LIBS) \
  $(SSL_LIBS) \
  $(CRYPTO_LIBS) \
  $(LIBCRYPTOPP) \
  $(LIBSECP256K1)

lux_cli_SOURCES = \
  lux-cli.cpp

lux_cli_CPPFLAGS = $(BITCOIN_INCLUDES)
#

# lux-tx binary #
lux_tx_LDADD = \
  $(LIBBITCOIN_UNIVALUE) \
  $(LIBBITCOIN_COMMON) \
  $(LIBBITCOIN_UTIL) \
  $(LIBBITCOIN_CRYPTO) \
  $(LIBSECP256K1) \
  $(BOOST_LIBS) \
  $(CRYPTO_LIBS) \
  $(LIBCRYPTOPP)

lux_tx_SOURCES = lux-tx.cpp
lux_tx_CPPFLAGS = $(BITCOIN_INCLUDES)
#
lux_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
lux_cli_SOURCES += lux-cli-res.rc
endif
lux_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if BUILD_BITCOIN_LIBS
include_HEADERS = script/bitcoinconsensus.h
libbitcoinconsensus_la_SOURCES = \
  allocators.cpp \
  primitives/transaction.cpp \
  crypto/hmac_sha512.cpp \
  crypto/scrypt.cpp \
  crypto/sha1.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  crypto/ripemd160.cpp \
  eccryptoverify.cpp \
  ecwrapper.cpp \
  hash.cpp \
  pubkey.cpp \
  script/script.cpp \
  script/interpreter.cpp \
  script/bitcoinconsensus.cpp \
  uint256.cpp \
  utilstrencodings.cpp

if GLIBC_BACK_COMPAT
  libbitcoinconsensus_la_SOURCES += compat/glibc_compat.cpp
  libbitcoinconsensus_la_SOURCES += compat/glibcxx_compat.cpp
endif

libbitcoinconsensus_la_LDFLAGS = -no-undefined $(RELDFLAGS)
libbitcoinconsensus_la_LIBADD = $(CRYPTO_LIBS) $(BOOST_LIBS) $(LIBSECP256K1)
libbitcoinconsensus_la_CPPFLAGS = $(CRYPTO_CFLAGS) -I$(builddir)/obj -I$(srcdir)/secp256k1/include -DBUILD_BITCOIN_INTERNAL
if USE_LIBSECP256K1
libbitcoinconsensus_la_LIBADD += secp256k1/libsecp256k1.la
endif
endif

CLEANFILES = leveldb/libleveldb.a leveldb/libmemenv.a
CLEANFILES += *.gcda *.gcno
CLEANFILES += compat/*.gcda compat/*.gcno
CLEANFILES += crypto/*.gcda crypto/*.gcno
CLEANFILES += primitives/*.gcda primitives/*.gcno
CLEANFILES += script/*.gcda script/*.gcno
CLEANFILES += univalue/*.gcda univalue/*.gcno
CLEANFILES += zmq/*.gcda zmq/*.gcno

DISTCLEANFILES = obj/build.h

EXTRA_DIST = leveldb

clean-local:
	-$(MAKE) -C leveldb clean
	-$(MAKE) -C secp256k1 clean
	rm -f leveldb/*/*.gcno leveldb/helpers/memenv/*.gcno
	-rm -f cryptopp/*.o
	-rm -f config.h

.rc.o:
	@test -f $(WINDRES)
	$(AM_V_GEN) $(WINDRES) -DWINDRES_PREPROC -i $< -o $@

.mm.o:
	$(AM_V_CXX) $(OBJCXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	  $(CPPFLAGS) $(AM_CXXFLAGS) $(QT_INCLUDES) $(CXXFLAGS)  -c -o $@ $<

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(abspath $(<D) $<)

include Makefile.cryptopp.include

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_QT
include Makefile.qt.include
endif

if ENABLE_QT_TESTS
include Makefile.qttest.include
endif
