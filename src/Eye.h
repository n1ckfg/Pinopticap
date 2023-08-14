#pragma once

#include "ofMain.h"

class Eye {

    public:
        Eye(string _hostName, string _sessionId, int _index);
        virtual ~Eye(){};
    
        string sessionId;
        string hostName;
		int index;
		int numSamples, numBlobSequences;

        struct EyeBlob {
            int index;
            float x;
            float y;
            int timestamp;
            int diff;
        };
        
		struct EyeBlobSequence {
			int index;
			vector<EyeBlob> blobs;
		};

		vector<EyeBlobSequence> blobSequences;

        struct EyeVideo {
            ofImage image;
            int timestamp;
            int diff;
        };
		
		vector<EyeVideo> videos;

		ofColor idColor, bgColor;

        void addBlob(int _index, float _x, float _y, int _timestamp, int _diff_timestamp);
		void drawBlob();
		int checkUniqueBlob(int _index);

		void addVideo(ofImage _image, int _timestamp, int _diff_timestamp);
		void drawVideo();

};

