
Building on Amazon Linux 2023
```
yum install cmake3 make chrpath openldap-clients grpc-devel gcc-c++ glib2-devel boost-devel -y
yum install openssl-devel zlib-devel protobuf-devel re2-devel krb5-devel systemd-devel -y
yum install systemd-rpm-macros dotnet-sdk-6.0 grpc-plugins -y
yum install bind-utils openldap openldap-clients awscli dotnet-runtime-6.0 -y
yum install jsoncpp-devel jsoncpp -y
yum install krb5-workstation -y
yum install cyrus-sasl-gssapi -y
yum install git -y

#get the code
git clone https://github.com/awsjohns/credentials-fetcher.git
cd credentials-fetcher/
git switch flatpak

#build the app
mkdir build
cd build
cmake ../
make -j 4

```

#after building on AL2023, we can package up with FlatPak so it can be distributed to 
#Amazon Linux 2 and run with FlatPak

```
#install Github utility
yum install yum-utils -y

yum-config-manager --add-repo https://cli.github.com/packages/rpm/gh-cli.repo
yum install gh -y

#install flatpak
yum install flatpak -y
flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

yum install flatpak-builder -y

flatpak install flathub org.freedesktop.Platform//22.08 org.freedesktop.Sdk//22.08 -y

cd /credentials-fetcher/flatpak
chmod +x ./make_credentials_fetcher_flatpak.sh 

#build the flatpack
./make_credentials_fetcher_flatpak.sh

```

Run credentials-fetcher flatpack app to test it
```
flatpak run --share=network  --filesystem=/var --env=CF_CRED_SPEC_FILE="/var/credentials-fetcher/credspec.json"  org.flatpak.credentialsfetcher -s gmsa-plugin-input
```


Build the bundle for distribution
```
flatpak build-export export build-dir

flatpak build-bundle export example.flatpak org.flatpak.credentials
```


Copy the flatpak bundle to S3 for distribute
```
aws s3 cp example.flatpak s3://your-bucket-name

```
