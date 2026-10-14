#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include "ofxOsc.h"
#include "ofxXmlSettings.h"
#include "ofxHTTP.h"
#include "ofxJSONElement.h"
#include "ofxCrypto.h"

#include "Eye.h"

class ofApp : public ofBaseApp {

    public:	
        void setup();
        void update();
        void draw();
        void exit();
        void keyPressed(int key);
            
        int width, height, appFramerate, camFramerate;
		int eyeWidth, eyeHeight, thumbWidth, thumbHeight, numColumns, numRows;

		string sessionId, hostName;

		string oscHost;
        int oscSendPort, oscReceivePort, streamPort, wsClientPort, wsServerPort, postPort;
        int timestamp;
		int dotSize;

        bool debug; // draw to local screen, default true
		bool videoEnabled = true;

        vector<Eye> eyes;
    
        ofFile file;
        ofxXmlSettings settings;

        bool sendOsc;  // send osc
        bool sendWs;  // send websockets
        bool sendHttp;  // serve web control panel
        bool sendMjpeg;  // send mjpeg stream

        ofxOscSender sender;
        ofxOscReceiver receiver;
        void sendOscBlobs(int index, float x, float y, float z);

        ofxHTTP::SimpleIPVideoServer streamServer;

        ofxHTTP::SimplePostServer postServer;
        void onHTTPPostEvent(ofxHTTP::PostEventArgs& evt);
        void onHTTPFormEvent(ofxHTTP::PostFormEventArgs& evt);
        void onHTTPUploadEvent(ofxHTTP::PostUploadEventArgs& evt);

        ofxHTTP::SimpleWebSocketServer wsServer;
        void onWebSocketOpenEvent(ofxHTTP::WebSocketEventArgs& evt);
        void onWebSocketCloseEvent(ofxHTTP::WebSocketCloseEventArgs& evt);
        void onWebSocketFrameReceivedEvent(ofxHTTP::WebSocketFrameEventArgs& evt);
        void onWebSocketFrameSentEvent(ofxHTTP::WebSocketFrameEventArgs& evt);
        void onWebSocketErrorEvent(ofxHTTP::WebSocketErrorEventArgs& evt);

        int checkSessionId(string _sessionId);
        int checkHostName(string _hostName);

        ofEasyCam cam; // add mouse controls for camera movement

        // ~ ~ ~ 3D ~ ~ ~
        struct Blob3d {
            int index;
            glm::vec3 position;
            int numEyes; // how many eyes agree on it
        };

        map<string, EyeCalibration> calibrations; // by hostName
        vector<BlobRay> blobRays;
        vector<Blob3d> blobs3d;
        double maxRayAngle; // radians a ray may miss a 3D blob by
        int maxBlobAge; // ms after which a 2D blob is no longer current
        float worldScale; // calibration units to grid units
        bool showEyes = true; // spacebar hides the 2D eyes

        void triangulateBlobs();
        void drawWorld();

};
