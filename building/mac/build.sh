
export PKG_CONFIG_PATH=/usr/local/opt/qt@5.5/lib/pkgconfig
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/usr/local/opt/zlib/lib/pkgconfig
export PATH=/usr/local/opt/qt@5.5/bin:$PATH
export QT_CFLAGS="-I/usr/local/opt/qt@5.5/lib/QtDBus.framework/Versions/5/Headers -I/usr/local/opt/qt@5.5/lib/QtWidgets.framework/Versions/5/Headers -I/usr/local/opt/qt@5.5/lib/QtNetwork.framework/Versions/5/Headers -I/usr/local/opt/qt@5.5/lib/QtGui.framework/Versions/5/Headers -I/usr/local/opt/qt@5.5/lib/QtCore.framework/Versions/5/Headers -I. -I/usr/local/opt/qt@5.5/mkspecs/macx-clang -F/usr/local/opt/qt@5.5/lib"
export QT_LIBS="-F/usr/local/opt/qt@5.5/lib -framework QtWidgets -framework QtGui -framework QtCore -framework DiskArbitration -framework IOKit -framework OpenGL -framework AGL -framework QtNetwork -framework QtDBus"

export LDFLAGS="-L/usr/local/opt/openssl/lib"
export CPPFLAGS="-I/usr/local/opt/openssl/include"
export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:usr/local/opt/openssl/lib/pkgconfig

./autogen.sh
./configure --with-gui=qt5 --with-boost=$(brew --prefix boost@1.60) --disable-tests --disable-zmq
make check
