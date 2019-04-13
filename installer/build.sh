#!/bin/bash

mkdir -p ROOT/tmp/SteelDriveII_X2/
cp "../SteelDriveII.ui" ROOT/tmp/SteelDriveII_X2/
cp "../SestoCalibrate.ui" ROOT/tmp/SteelDriveII_X2/
cp "../PrimaLuceLab.png" ROOT/tmp/SteelDriveII_X2/
cp "../focuserlist PrimaLuceLab.txt" ROOT/tmp/SteelDriveII_X2/
cp "../build/Release/libSteelDriveII.dylib" ROOT/tmp/SteelDriveII_X2/

if [ ! -z "$installer_signature" ]; then
# signed package using env variable installer_signature
pkgbuild --root ROOT --identifier org.rti-zone.SteelDriveII_X2 --sign "$installer_signature" --scripts Scripts --version 1.0 SteelDriveII_X2.pkg
pkgutil --check-signature ./SteelDriveII_X2.pkg
else
pkgbuild --root ROOT --identifier org.rti-zone.SteelDriveII_X2 --scripts Scripts --version 1.0 SteelDriveII_X2.pkg
fi

rm -rf ROOT
