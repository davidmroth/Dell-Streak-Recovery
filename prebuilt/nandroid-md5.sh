#!/sbin/sh
cd $1

md5sum *.img  2> /dev/null 1> nandroid.md5
ERROR=$?

if [ -e .*.img ]; then
    md5sum .*.img 2> /dev/null 1>> nandroid.md5 
else 
    true
fi

return $((ERROR | $?)) 
