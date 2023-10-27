#!/bin/sh

set -x

if [ ! -f ../build/credentials-fetcherd ];
then
   echo "**ERROR: Please copy the credentials-fetcher binary to this directory"
   exit 1
fi

LIB_FILES=$(ldd ../build/credentials-fetcherd | grep -v linux-vdso | grep -v ld-linux | awk '{print $3}')

rm -rf libs
mkdir -p libs


echo "$LIB_FILES" | sed 's/ /\n/g' > tmpfile.txt
while read -r LINE
do
  if [ "$LINE" != "/lib64/libc.so.6" ]; then
    cp $LINE libs
  fi
done < tmpfile.txt

LIB_FILES=$(ldd /usr/bin/dig | grep -v linux-vdso | grep -v ld-linux | awk '{print $3}')
echo "$LIB_FILES" | sed 's/ /\n/g' > tmpfile.txt
while read -r LINE
do
  if [ "$LINE" != "/lib64/libc.so.6" ]; then
    cp $LINE libs
  fi
done < tmpfile.txt

LIB_FILES=$(ldd /usr/bin/ldapsearch | grep -v linux-vdso | grep -v ld-linux | awk '{print $3}')
echo "$LIB_FILES" | sed 's/ /\n/g' > tmpfile.txt
while read -r LINE
do
  if [ "$LINE" != "/lib64/libc.so.6" ]; then
    cp $LINE libs
  fi
done < tmpfile.txt


#cp -r /usr/lib64/sasl2/* libs/

\rm -rf build-dir
flatpak-builder build-dir org.flatpak.credentialsfetcher.yml
flatpak-builder --share=network --user --install --force-clean build-dir org.flatpak.credentialsfetcher.yml
#flatpak run  --filesystem=home org.flatpak.credentialsfetcher

#build the bundle for distribution

flatpak build-export export build-dir

flatpak build-bundle export credentialsfetcher.flatpak org.flatpak.credentialsfetcher

#cleanup
rm -rf libs
rm -rf build-dir
rm tmpfile.txt
rm -rf export
rm -rf .flatpak-builder
exit 0
