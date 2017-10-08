<b> LUXoin - New Implemented PHI1612 Algorithm </b><br /><br />

©LUX 2017 | by 216k155 - @Dra
-----------------------
Development process
===========================

01/10/2017 -----> PHI1612 hash initilized, replaced novacoin-qt, json, add intallised-dependencies.sh, changed qt/res,etc....

02/10/2017 -----> Added echo hash into PHI1612

03/10/2017 -----> New Phi1612 hashing algorithm completed
Note: Not finished yet, still need some work. 

04/10/2017 ------> testnet trial version released

06/10/2017 ------> Final test before launch ( 06/10/2017 - 09/10/2017)

07/10/2017 ------> Changed bhcoin to lux ( Trial Version v3)

09/10/2017 ------> Updated New Trial Version Lux - PHI | PoW/PoS Hybrid - Parallel Masternode Implemented | I2pd Networks Remote Activation
-------------------------------------------------------------------

<b> ccminer & sgminer for phi1612 algo </b> 

Haven't test Phi1612 hash using GPU yet. I appreciate that if anyone can give me a hand to add phi1612 hasing into ccminer & sgminer.




# Install
=========
-Unix daemon compile:

	cd lux

	sudo sh install-dependencies.sh install

	cd src/leveldb

	chmod +x build_detect_platform

	make clean && make libleveldb.a libmemenv.a

	cd ..

	make -f makefile.unix 

-Unix QT wallet compile:

	cd lux

	qmake USE_QRCODE=1

	make

