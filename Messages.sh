#! /bin/bash

$EXTRACT_TR_STRINGS `find src/ \
                          -name \*.cpp -o \
                          -name \*.h -o \
                          -name \*.ui` \
                    -o $podir/trojita_qt.pot

$XGETTEXT `find src/ -name \*.cpp -o -name \*.h` -o $podir/plasma-impresario.pot

