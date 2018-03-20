
cp src/LUXd .
cp src/qt/LUX-qt .
strip LUXd
strip LUX-qt
zip release_${LUX_PLATFORM}.zip LUXd LUX-qt

sudo easy_install appscript

# fix for the 'Error: No file at /opt/local/lib/mysql55/mysql/libmysqlclient.18.dylib' issue
brew install mysql
pwd
cd /usr/local/opt/qt@5.5/plugins/sqldrivers
echo "before:"
otool -L libqsqlmysql.dylib
install_name_tool -change /opt/local/lib/mysql55/mysql/libmysqlclient.18.dylib /usr/local/Cellar/mysql/5.7.11/lib/libmysqlclient.20.dylib libqsqlmysql.dylib
echo "after:"
otool -L libqsqlmysql.dylib
cd -

ls -al VER*
make deploy
ls -al VER*

# for pushing releases
brew install ruby
