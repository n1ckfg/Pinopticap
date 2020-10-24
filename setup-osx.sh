#!/bin/bash

SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE" # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"

cd $DIR

cd ../../../addons
git clone https://github.com/n1ckfg/ofxHTTP
git clone https://github.com/n1ckfg/ofxIO
git clone https://github.com/n1ckfg/ofxMediaType
git clone https://github.com/n1ckfg/ofxNetworkUtils
git clone https://github.com/n1ckfg/ofxSSLManager
git clone https://github.com/n1ckfg/ofxSSLManager
git clone https://github.com/n1ckfg/ofxJSON
git clone https://github.com/n1ckfg/ofxCrypto

cd $DIR
