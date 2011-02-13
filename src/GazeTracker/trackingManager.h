/*
 *  trackingManager.h
 *  eyeWriterCam
 *
 *  Created by sugano on 11/01/25.
 *
 */

#ifndef _TRACKSCENE_H
#define _TRACKSCENE_H

#include <deque>

#include "inputManager.h"
#include "GaussianProcess.h"

#include "cv.h"

#include "asmmodel.h"
#include "modelfile.h"
#include "util.h"

class trackingManager {
	
public:
	trackingManager();
	~trackingManager();
	
	void setup();
	void update();
	void draw();
	
	bool bBeenCalibrated;
	ofPoint getEyePoint(){ return ofPoint(gazePt.x, gazePt.y); };
	
	void keyPressed(int key);
	
private:
	void grabFrame();
	
	IplImage *sourceImage, *colorImage;
	ofxCvColorImage dispImage;
	void updateDispImage();
	
	static const int eyeImageWidth = 80; // for bEye (combined image of both eyes)
	static const int eyeImageHeight = 20;
	Mat bEye, lEye, rEye;
	bool grabEyeImages();
	
	inputManager IM;
	
	ofTrueTypeFont dispFont;
	
	enum trackingModes{
		TM_MODE_DETECTION,
		TM_MODE_TRACKING,
		TM_MODE_CALIBRATION,
	} currentMode;
	
	// eye detection
	bool detectEyes();
	static const int numEyePositions = 4;
	static const int eyeTemplateSize = 20;
	static const int maxEyeHistory = 10;
	std::deque<cv::Point2f> eyeHistories[numEyePositions];
	cv::CascadeClassifier faceCascade;
	ASMModel asmModel;
	void readASMModel(ASMModel &asmModel, string modelPath);
	cv::Point2f eyePositions[numEyePositions];
	
	// eye tracking
	bool trackEyes();
	static const int searchWindowSize = 30;
	Mat initTemplates[numEyePositions], prevTemplates[numEyePositions];
	
	// calibration
	static const int calibPointsNumX = 7;
	static const int calibPointsNumY = 7;
	int calibPointsNumAll;
	int calibPtIdx;
	static const int calibPointsRadMax = 200;
	static const float calibPointsRadSpeed = 250;
	int calibPtRad;
	int calibAreaX, calibAreaY;
	std::vector<cv::Point2f> calibrationPoints;
	std::vector<cv::Mat> calibrationEyes;
	
	// gaze estimator
	cv::Point2f gazePt;
	static const int maxGazeHistory = 10;
	std::deque<cv::Point2f> gazeHistories;
	CGazeEstimator gazeEstimator;
};

#endif