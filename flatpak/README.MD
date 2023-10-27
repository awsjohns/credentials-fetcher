#after building on AL2023, we can package up with FlatPak so it can be distributed to 
#Amazon Linux 2 and run with FlatPak

```
#install Github utility
yum install yum-utils -y

yum-config-manager --add-repo https://cli.github.com/packages/rpm/gh-cli.repoyum install gh -y#install flatpakyum install flatpak -y
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

yum install flatpak-builder -y

flatpak install flathub org.freedesktop.Platform//22.08 org.freedesktop.Sdk//22.08 -y

#switch to the credentials-fetcher to PR that has flatpak support
cd /credentials-fetcher/
#gh auth login
#gh pr checkout 71


cd /credentials-fetcher/flatpak
#cp build/credentials-fetcherd flatpak/
chmod +x ./make_credentials_fetcher_flatpak.sh 

#build the flatpack
./make_credentials_fetcher_flatpak.sh

#run credentials-fetcher flatpack app to test it

#flatpak run --filesystem=/var org.flatpak.credentialsfetcher
flatpak run --share=network  --filesystem=/var --env=SASL_PATH=/app/lib/sasl2 org.flatpak.credentialsfetcher

#build the bundle for distribution
flatpak build-export export build-dir

flatpak build-bundle export example.flatpak org.flatpak.credentials

#copy the flatpak bundle to S3 to distribute
aws s3 cp example.flatpak s3://your-bucket-name

```
