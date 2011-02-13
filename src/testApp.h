#ifndef _TEST_APP
#define _TEST_APP


#include "ofMain.h"

#include "trackingManager.h"

#include "buttonTrigger.h"
#include "typingScene.h"
#include "pongScene.h"
#include "eyePlotterTestApp.h"

static const int num_modes = 5;

enum{
	MODE_TRACKING,
	MODE_TEST,
	MODE_DRAW,
	MODE_TYPING,
	MODE_PONG,
};

class testApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void keyPressed  (int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		
	private:
		//----- scenes
		trackingManager		TM;
		buttonTrigger		BT;
		eyePlotterTestApp	eyeApp;
		typingScene			typeScene;
		pongScene			ponger;
		
		ofPoint eyeSmoothed;
		ofPoint screenPoint;
		float waitFpsTime;
		float timeSince;
		
		int mode; 
		
		bool bMouseSimulation;
		
		//------ drawing
		void drawHelp();
		
		static const float rotSmooth = .9;

};

#endif
