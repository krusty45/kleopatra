#! /bin/sh
$EXTRACTRC `find . -name \*.rc -o -name \*.ui -o -name \*.kcfg` >> rc.cpp || exit 11
$XGETTEXT `find . -name \*.cc -o -name \*.cpp -o -name \*.h | grep -v '/tests/' | grep -v '/autotests/' | grep -v '/kwatchgnupg/'` -o $podir/kleopatra.pot
rm -f rc.cpp
