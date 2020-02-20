export ARCH=x86_64
export QT_SELECT=5

cd /app/workspace/atsdb/
mkdir -p appimage/appdir/bin/

cp /usr/bin/atsdb_client appimage/appdir/bin/
mkdir -p appimage/appdir/lib/
cp /usr/lib/libatsdb.a appimage/appdir/lib/

cp -r /usr/lib64/osgPlugins-3.4.1 appimage/appdir/lib/
#cp -r /usr/lib/osgPlugins-3.6.4 appimage/appdir/lib/
cp /usr/lib64/libosgEarth* appimage/appdir/lib/
cp /usr/lib64/osgdb_* appimage/appdir/lib/

#chrpath -r '$ORIGIN' appimage/appdir/lib/*
#chrpath -r '$ORIGIN/osgPlugins-3.4.1' appimage/appdir/lib/osgPlugins-3.4.1/*

mkdir -p appimage/appdir/atsdb/
cp -r data appimage/appdir/atsdb/
cp -r conf appimage/appdir/atsdb/

/app/tools/linuxdeployqt-continuous-x86_64.AppImage --appimage-extract-and-run appimage/appdir/atsdb.desktop -appimage -bundle-non-qt-libs -verbose=2 
#-qmake=/usr/lib/x86_64-linux-gnu/qt5/bin/qmake 

cd /app/workspace/atsdb/docker
