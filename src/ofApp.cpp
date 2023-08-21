#include "ofApp.h"

#include "../../common/src/Pinopticon.hpp"
#include "../../common/src/Pinopticon_Http.hpp"
#include "../../common/src/Pinopticon_Osc.hpp"

using namespace cv;
using namespace ofxCv;
using namespace Pinopticon;

//--------------------------------------------------------------
void ofApp::setup() {
    settings.loadFile("settings.xml");
    
    ofSetVerticalSync(false);
    ofHideCursor();

    appFramerate = settings.getValue("settings:app_framerate", 60);
    camFramerate = settings.getValue("settings:cam_framerate", 30);
    ofSetFrameRate(appFramerate);   

	numColumns = 3;
	numRows = 3;
	thumbWidth = 120;
	thumbHeight = 90;
	eyeWidth = 320;
	eyeHeight = 240;
	width = eyeWidth * numColumns; // settings.getValue("settings:width", 1280);
	height = eyeHeight * numRows; // settings.getValue("settings:height", 720);
	ofSetWindowShape(width, height);

    debug = (bool) settings.getValue("settings:debug", 1);

    oscHost = settings.getValue("settings:osc_host", "127.0.0.1");
    oscSendPort = settings.getValue("settings:osc_send_port", 7114);
    oscReceivePort = settings.getValue("settings:osc_receive_port", 7110);
 
    sendOsc = (bool)settings.getValue("settings:send_osc", 1);
    sendWs = (bool)settings.getValue("settings:send_ws", 1);
    sendHttp = (bool)settings.getValue("settings:send_http", 1);
    sendMjpeg = (bool)settings.getValue("settings:send_mjpeg", 1);

    debug = (bool) settings.getValue("settings:debug", 1);

	dotSize = 10;

	// ~ ~ ~   get a persistent name for this computer   ~ ~ ~
#ifdef TARGET_LINUX_ARM
    ofSystem("cp /etc/hostname " + ofToDataPath("DocumentRoot/js/"));
    hostName = getHostName();
#else
	hostName = "unknown";
#endif
    sessionId = getSessionId();

    // * stream video *
    int maxClientConnections = settings.getValue("settings:max_stream_connections", 5); // default 5
    int maxClientBitRate = settings.getValue("settings:max_stream_bitrate", 512); // default 1024
    int maxClientFrameRate = settings.getValue("settings:max_stream_framerate", 30); // default 30
    int maxClientQueueSize = settings.getValue("settings:max_stream_queue", 10); // default 10
    setupMjpeg(streamServer, streamPort, maxClientConnections, maxClientBitRate, maxClientFrameRate, maxClientQueueSize, width, height, "live_view.html");

    // * post form *
    setupHttp(this, postServer, postPort, "result.html");

    // * websockets *
    // events: connect, open, close, idle, message, broadcast
    setupWsServer(this, wsServer, wsPort);

    setupOscSender(sender, oscHost, oscSendPort);
    setupOscReceiver(receiver, oscReceivePort);
}

//--------------------------------------------------------------
void ofApp::update() {
    timestamp = getTimestamp();
    
    // check for waiting messages
    while (receiver.hasWaitingMessages()) {
        // get the next message
        ofxOscMessage msg;
        receiver.getNextMessage(msg);

        string newHostName = msg.getArgAsString(0);
        string newSessionId = msg.getArgAsString(1);

        int whichOne = checkHostName(newHostName);
        //int whichOne = checkSessionId(newsessionId);

        if (whichOne == -1) {
			whichOne = eyes.size();
			Eye eye = Eye(newHostName, newSessionId, whichOne);
			eyes.push_back(eye);
            cout << "New Eye detected: " << newHostName << " " << newSessionId << endl;
        }
        
        Eye eye = eyes[whichOne];

        if (eye.sessionId != newSessionId) {
            eyes[whichOne].sessionId = newSessionId;
            cout << "Eye reconnected: " << eye.hostName << " " << eye.sessionId << endl;
        }

        //cout << "New OSC message " << msg.getAddress() << " received from " << eye.hostName << " " << eye.sessionId << endl;

        if (msg.getAddress() == "/blob") {
            int index = msg.getArgAsInt32(2);
            float x = msg.getArgAsFloat(3);
            float y = msg.getArgAsFloat(4);
            int msg_timestamp = msg.getArgAsInt32(5);
            
            eyes[whichOne].addBlob(index, x, y, msg_timestamp, timestamp);
        } else if (msg.getAddress() == "/video" && videoEnabled) {
            ofBuffer buffer = msg.getArgAsBlob(2);
            ofImage image;
            bufferToImage(buffer, image, thumbWidth, thumbHeight, false);
            //cout << "Received image " << image.getWidth() << "x" << image.getHeight() << endl;
            int msg_timestamp = msg.getArgAsInt32(3);

            eyes[whichOne].addVideo(image, msg_timestamp, timestamp);
        } else if (msg.getAddress() == "/contour") {
            /*
            ofBuffer buffer = msg.getArgAsBlob(2);
            ofImage image;
            bufferToImage(buffer, image, imageWidth, imageHeight, true);
            cout << "Received image " << image.getWidth() << "x" << image.getHeight() << endl;
            int msg_timestamp = msg.getArgAsInt32(3);

            eyes[whichOne].addVideo(image, msg_timestamp, timestamp);
            */
        }
    }
}

//--------------------------------------------------------------
void ofApp::draw() {
	if (debug) {
		ofBackground(0);

        cam.begin();
        ofEnableDepthTest();
        ofDrawGrid(20, 10, true, true, true, true);
        ofDisableDepthTest();
        cam.end();

		for (int i=0; i<eyes.size(); i++) {
			int originX = eyeWidth * (i % numColumns);
			int originY = eyeHeight * (i / numRows);

			ofSetColor(eyes[i].bgColor);
			ofRect(originX, originY, eyeWidth, eyeHeight);

			if (videoEnabled) {
				eyes[i].drawVideo(originX, originY, eyeWidth, eyeHeight);
			}

			for (int j = 0; j < eyes[i].blobSequences.size(); j++) {
				for (int k = 0; k < eyes[i].blobSequences[j].blobs.size(); k++) {
					int x = eyes[i].blobSequences[j].blobs[k].x * eyeWidth;
					int y = eyes[i].blobSequences[j].blobs[k].y * eyeHeight;

					x += originX;
					y += originY;
					int idx = eyes[i].blobSequences[j].blobs[k].index;			

					ofSetColor(eyes[i].idColor);
					ofCircle(x, y, dotSize);
					
					ofSetColor(eyes[i].bgColor);
					string msg = ofToString(idx);
					ofDrawBitmapString(msg, x - dotSize / 2, y + dotSize / 2);

					ofDrawBitmapStringHighlight(eyes[i].hostName, originX, originY + eyeHeight, ofColor::black, ofColor::yellow);
				}
			}
		}

        stringstream info;
        info << width << "x" << height << " @ " << ofGetFrameRate() << "fps" << " / " << timestamp << "\n";
        ofDrawBitmapStringHighlight(info.str(), 10, 10, ofColor::black, ofColor::yellow);
    }
}

int ofApp::checkSessionId(string _sessionId) {
    int idx = -1;
    for (int i=0; i<eyes.size(); i++) {
        if (_sessionId == eyes[i].sessionId) {
            idx = i;
            break;
        }
    }
    return idx;
}

int ofApp::checkHostName(string _hostName) {
    int idx = -1;
    for (int i = 0; i < eyes.size(); i++) {
        if (_hostName == eyes[i].hostName) {
            idx = i;
            break;
        }
    }
    return idx;
}

void ofApp::onWebSocketOpenEvent(ofxHTTP::WebSocketEventArgs& evt) {
    cout << "Websocket connection opened." << evt.connection().clientAddress().toString() << endl;
}

void ofApp::onWebSocketCloseEvent(ofxHTTP::WebSocketCloseEventArgs& evt) {
    cout << "Websocket connection closed." << evt.connection().clientAddress().toString() << endl;
}

void ofApp::onWebSocketFrameReceivedEvent(ofxHTTP::WebSocketFrameEventArgs& evt) {
    cout << "Websocket frame was received:" << evt.connection().clientAddress().toString() << endl;
    string msg = evt.frame().getText();
    cout << msg << endl;

    if (msg == "take_photo") {
        //takePhoto();
    } else if (msg == "stream_photo") {
        //streamPhoto();
    }
}

void ofApp::onWebSocketFrameSentEvent(ofxHTTP::WebSocketFrameEventArgs& evt) {
    cout << "Websocket frame was sent." << endl;
}


void ofApp::onWebSocketErrorEvent(ofxHTTP::WebSocketErrorEventArgs& evt) {
    cout << "Websocket Error." << evt.connection().clientAddress().toString() << endl;
}

// ~ ~ ~ POST ~ ~ ~
void ofApp::onHTTPPostEvent(ofxHTTP::PostEventArgs& args) {
    ofLogNotice("ofApp::onHTTPPostEvent") << "Data: " << args.getBuffer().getText();

    //takePhoto();
}


void ofApp::onHTTPFormEvent(ofxHTTP::PostFormEventArgs& args) {
    ofLogNotice("ofApp::onHTTPFormEvent") << "";
    ofxHTTP::HTTPUtils::dumpNameValueCollection(args.getForm(), ofGetLogLevel());

    //takePhoto();
}


void ofApp::onHTTPUploadEvent(ofxHTTP::PostUploadEventArgs& args) {
    std::string stateString = "";

    switch (args.getState()) {
    case ofxHTTP::PostUploadEventArgs::UPLOAD_STARTING:
        stateString = "STARTING";
        break;
    case ofxHTTP::PostUploadEventArgs::UPLOAD_PROGRESS:
        stateString = "PROGRESS";
        break;
    case ofxHTTP::PostUploadEventArgs::UPLOAD_FINISHED:
        stateString = "FINISHED";
        break;
    }

    ofLogNotice("ofApp::onHTTPUploadEvent") << "";
    ofLogNotice("ofApp::onHTTPUploadEvent") << "         state: " << stateString;
    ofLogNotice("ofApp::onHTTPUploadEvent") << " formFieldName: " << args.getFormFieldName();
    ofLogNotice("ofApp::onHTTPUploadEvent") << "orig. filename: " << args.getOriginalFilename();
    ofLogNotice("ofApp::onHTTPUploadEvent") << "     filename: " << args.getFilename();
    ofLogNotice("ofApp::onHTTPUploadEvent") << "     fileType: " << args.getFileType().toString();
    ofLogNotice("ofApp::onHTTPUploadEvent") << "# bytes xfer'd: " << args.getNumBytesTransferred();
}