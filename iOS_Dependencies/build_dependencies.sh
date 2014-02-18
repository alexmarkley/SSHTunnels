#!/bin/sh

GLOBAL_OUTDIR="/opt/ios-libs"
OUTDIR=`pwd`"/outdir_temp"

IOS_BASE_SDK="7.0"
IOS_DEPLOY_TGT="6.0"

XCODE_ROOT=`xcode-select --print-path`

setenv_all()
	{
	# Using CLANG
	export CFLAGS="$CFLAGS -I$GLOBAL_OUTDIR/include -I$SDKROOT/usr/include -fobjc-abi-version=2"
	export LDFLAGS="$LDFLAGS -L$GLOBAL_OUTDIR/lib -L$SDKROOT/usr/lib -fobjc-abi-version=2"
	CLANG_BINARY=`xcrun -f clang`
	export CXX=$CLANG_BINARY
	export CC=$CLANG_BINARY
	export OBJC=$CLANG_BINARY
	
	unset CPP
	unset CXXCPP
	
	export LD=`xcrun -f ld`
	export AR=`xcrun -f ar`
	export AS=`xcrun -f as`
	export NM=`xcrun -f nm`
	export RANLIB=`xcrun -f ranlib`
	export STRIP=`xcrun -f strip`
    	
	export CPPFLAGS=$CFLAGS
	export CXXFLAGS=$CFLAGS
	
	#Report
	echo ==============================
	echo CFLAGS="${CFLAGS}"
	echo LDFLAGS="${LDFLAGS}"
	echo CC="${CC}"
	echo LD="${LD}"
	echo AR="${AR}"
	echo AS="${AS}"
	echo NM="${NM}"
	echo RANLIB="${RANLIB}"
	echo STRIP="${STRIP}"
	echo ==============================
	}

setenv_arm()
	{
	ARCH=$1
	
	unset DEVROOT SDKROOT CFLAGS CC LD CPP CXX AR AS NM CXXCPP RANLIB LDFLAGS CPPFLAGS CXXFLAGS
	
	export DEVROOT=$XCODE_ROOT/Platforms/iPhoneOS.platform/Developer
	export SDKROOT=$DEVROOT/SDKs/iPhoneOS$IOS_BASE_SDK.sdk
	
	export CFLAGS="-arch $ARCH -D__arm__ -U__i386__ -U__x86_64__ -pipe -no-cpp-precomp -isysroot $SDKROOT -miphoneos-version-min=$IOS_DEPLOY_TGT"
	export LDFLAGS="$CFLAGS"
	
	setenv_all
	}

#setenv_i386()
#	{
#	unset DEVROOT SDKROOT CFLAGS CC LD CPP CXX AR AS NM CXXCPP RANLIB LDFLAGS CPPFLAGS CXXFLAGS
#	
#	export DEVROOT=$XCODE_ROOT/Platforms/iPhoneSimulator.platform/Developer
#	export SDKROOT=$DEVROOT/SDKs/iPhoneSimulator$IOS_BASE_SDK.sdk
#	
#	export CFLAGS="-arch i386 -pipe -no-cpp-precomp -isysroot $SDKROOT -miphoneos-version-min=$IOS_DEPLOY_TGT"
#	export LDFLAGS="$CFLAGS"
#	
#	setenv_all
#	}

create_outdir_lipo()
	{
	for LIB_armv7s in `find $OUTDIR/armv7s -name "lib*\.a"`
		do
		LIB_armv7=`echo $LIB_armv7s | sed "s/armv7s/armv7/g"`
		LIB_arm64=`echo $LIB_armv7s | sed "s/armv7s/arm64/g"`
		LIB=`echo $LIB_armv7s | sed "s/armv7s\///g"`
		lipo -arch armv7s $LIB_armv7s -arch armv7 $LIB_armv7 -arch arm64 $LIB_arm64 -create -output $LIB
		done
	}

#merge_libfiles()
#	{
#	DIR=$1
#	LIBNAME=$2
#	
#	cd $DIR
#	for i in `find . -name "lib*.a"`; do
#		$AR -x $i
#	done
#	$AR -r $LIBNAME *.o
#	rm -rf *.o __*
#	cd -
#	}

actually_build()
	{
	BUILD_LIB=$1
	LIBDIR=$2
	
	cd $BUILD_LIB
	rm -rf $OUTDIR
	
	for ARCH in armv7s armv7 arm64
		do
		mkdir -p $OUTDIR/$ARCH
		make clean 2> /dev/null
		make distclean 2> /dev/null
		setenv_arm $ARCH
		./configure --prefix=$GLOBAL_OUTDIR --host=arm-apple-darwin --enable-shared=no || exit
		make || exit
		cp -rvf $LIBDIR/lib*.a $OUTDIR/$ARCH
		done
	
	create_outdir_lipo
	echo "We are probably about to ask you for your password. This is so we can place things in $GLOBAL_OUTDIR"
	sudo make install
	sudo cp -rvf $OUTDIR/lib*.a $GLOBAL_OUTDIR/lib
	cd -
	}

# eXpat
EXPAT=expat-2.1.0
/bin/rm -r -f "$EXPAT"*
curl -O http://colocrossing.dl.sourceforge.net/project/expat/expat/2.1.0/expat-2.1.0.tar.gz
tar xzvf "$EXPAT".tar.gz
cd "$EXPAT" ; patch -p1 <../expat-readfilemap.patch ; cd ..
actually_build "$EXPAT" .libs

#freetype
#FREETYPE=freetype-2.4.10
#actually_build $FREETYPE "" objs/.libs
#echo "We are probably about to ask you for your password. This is so we can place things in $GLOBAL_OUTDIR"
#sudo mkdir -p $GLOBAL_OUTDIR/include
#sudo cp -rvf $FREETYPE/include/* $GLOBAL_OUTDIR/include/
#sudo mkdir -p $GLOBAL_OUTDIR/lib/pkgconfig
#sudo cp $FREETYPE/builds/unix/freetype2.pc $GLOBAL_OUTDIR/lib/pkgconfig/
#sudo mkdir -p $GLOBAL_OUTDIR/bin
#sudo cp $FREETYPE/builds/unix/freetype-config $GLOBAL_OUTDIR/bin/
#sudo chmod +x $GLOBAL_OUTDIR/bin/freetype-config

