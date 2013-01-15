#!/bin/sh

LIST=`find * -iname "*.dot" `

for FILE in $LIST; do
        BASENAME=`basename $FILE .dot`;
        dot -Tpng -o graphs/$BASENAME.png $FILE;
        rm -f $FILE;
done

