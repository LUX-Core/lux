#!/bin/sh

if [ ! "$1" ] 
then
   echo "Qt dir not set";
   exit
else
   if [ ! "$2" ]
   then
	echo "qmake executable not set";
	exit
   fi
fi

"$1"/Tools/QtInstallerFramework/3.0/bin/binarycreator --offline-only -c ../config/config.xml -p ../packages/ -t "$1"/Tools/QtInstallerFramework/3.0/bin/installerbase OmenInstaller
