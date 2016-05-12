#!/bin/bash

# Ugly way to get absolute path of this directory
pushd `dirname $0` > /dev/null
SCRIPTPATH=`pwd`
popd > /dev/null

# List of paths
BASEDIR=$SCRIPTPATH
export OCS_BUILD_DIR_PATH=$BASEDIR/build
export OCS_DEPLOY_DIR_PATH=$OCS_BUILD_DIR_PATH/deploy

# Print used environment
echo
echo OCS Build Environment
echo ---------------------
echo OCS_BUILD_DIR_PATH  : $OCS_BUILD_DIR_PATH
echo OCS_DEPLOY_DIR_PATH : $OCS_DEPLOY_DIR_PATH
echo ---------------------
echo 

# Run CMake
cd $BASEDIR
mkdir $OCS_BUILD_DIR_PATH
cd $OCS_BUILD_DIR_PATH
cmake -DCMAKE_INSTALL_PREFIX="$OCS_DEPLOY_DIR_PATH" -DCMAKE_BUILD_TYPE=Release ..

# Run Make + Install
make && make install

# Copy dependencies
ARCH=$(uname -m)
if [ $ARCH = "x86_32" ]; then 
	cp /usr/lib/i386-linux-gnu/libstdc++.so.6 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/i386-linux-gnu/libgcc_s.so.1 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/i386-linux-gnu/libc.so.6 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/i386-linux-gnu/libpthread.so.0 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/i386-linux-gnu/libdl.so.2 $OCS_DEPLOY_DIR_PATH/server/
	cp /usr/lib/i386-linux-gnu/libgthread-2.0.so.0 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/i386-linux-gnu/librt.so.1 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/i386-linux-gnu/libglib-2.0.so.0 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/i386-linux-gnu/libm.so.6 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/ld-linux.so.2 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/i386-linux-gnu/libpcre.so.3 $OCS_DEPLOY_DIR_PATH/server/
elif [ $ARCH = "x86_64" ]; then
	cp /usr/lib/x86_64-linux-gnu/libstdc++.so.6 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/x86_64-linux-gnu/libgcc_s.so.1 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/x86_64-linux-gnu/libc.so.6 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/x86_64-linux-gnu/libpthread.so.0 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/x86_64-linux-gnu/libdl.so.2 $OCS_DEPLOY_DIR_PATH/server/
	cp /usr/lib/x86_64-linux-gnu/libgthread-2.0.so.0 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/x86_64-linux-gnu/librt.so.1 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/x86_64-linux-gnu/libglib-2.0.so.0 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/x86_64-linux-gnu/libm.so.6 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib64/ld-linux-x86-64.so.2 $OCS_DEPLOY_DIR_PATH/server/
	cp /lib/x86_64-linux-gnu/libpcre.so.3 $OCS_DEPLOY_DIR_PATH/server/
fi

cp $QTDIR/lib/libQt5Core.so.5 $OCS_DEPLOY_DIR_PATH/server/
cp $QTDIR/lib/libQt5Network.so.5 $OCS_DEPLOY_DIR_PATH/server/
cp $QTDIR/lib/libQt5WebSockets.so.5 $OCS_DEPLOY_DIR_PATH/server/
cp $QTDIR/lib/libicui18n.so.53 $OCS_DEPLOY_DIR_PATH/server/
cp $QTDIR/lib/libicuuc.so.53 $OCS_DEPLOY_DIR_PATH/server/
cp $QTDIR/lib/libicudata.so.53 $OCS_DEPLOY_DIR_PATH/server/

cp $BASEDIR/projects/videoserver/res/scripts/start.sh $OCS_DEPLOY_DIR_PATH/server/videoserver.sh
cp $BASEDIR/projects/videoserver/res/scripts/start-initd.sh $OCS_DEPLOY_DIR_PATH/server/videoserver-initd.sh


# Copy LDD dependencies (DO NOT USE YET!)
if false; then

	AppBasePath=$OCS_DEPLOY_DIR_PATH/server
	AppFilePath=$OCS_DEPLOY_DIR_PATH/server/videoserver
	echo "Collecting the shared library dependencies for $AppFilePath..."
	deps=$(ldd $AppFilePath | awk 'BEGIN{ORS=" "}$1\
	~/^\//{print $1}$3~/^\//{print $3}'\
	 | sed 's/,$/\n/')
	 
	for dep in $deps
	do
		echo "Copy $dep to $AppBasePath"
	done
fi
