sudo apt-get update

sudo apt-get install -y xvfb

# NTP
sudo apt-get install -y htpdate iputils-clockdiff
sudo timedatectl set-ntp true
timedatectl status

# https://github.com/Azure/azure-iot-sdk-c/issues/265

DIR=$PWD

cd ../../../addons
#git clone https://github.com/n1ckfg/ofxCvPiCam
#git clone https://github.com/n1ckfg/ofxOMXCamera
git clone https://github.com/n1ckfg/ofxHTTP
git clone https://github.com/n1ckfg/ofxIO
git clone https://github.com/n1ckfg/ofxMediaType
git clone https://github.com/n1ckfg/ofxNetworkUtils
git clone https://github.com/n1ckfg/ofxSSLManager
git clone https://github.com/n1ckfg/ofxSSLManager
git clone https://github.com/n1ckfg/ofxJSON
git clone https://github.com/n1ckfg/ofxJSONRPC
git clone https://github.com/n1ckfg/ofxCrypto

cd $DIR
