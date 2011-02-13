/*
 *  trackingManager.cpp
 *  eyeWriterCam
 *
 *  Created by sugano on 11/01/25.
 *
 */

#include "trackingManager.h"

using namespace std;
using namespace cv;

trackingManager::trackingManager(){
	sourceImage = NULL;
	colorImage = NULL;
}

trackingManager::~trackingManager(){
	if(sourceImage != NULL) cvReleaseImage(&sourceImage);
	if(colorImage != NULL) cvReleaseImage(&colorImage);
}

void trackingManager::setup(){
	IM.setup();
	
	//--- set up tracking
	printf("VideoWidth, VideoHeight = %d, %d \n", IM.width, IM.height);
	
	sourceImage = cvCreateImage(cvSize(IM.width, IM.height), IPL_DEPTH_8U, 1);
	colorImage = cvCreateImage(cvSize(IM.width, IM.height), IPL_DEPTH_8U, 3);
	dispImage.allocate(IM.width, IM.height);
	
	bEye.create(eyeImageHeight, eyeImageWidth, CV_8UC1);
	lEye.create(eyeImageHeight, eyeImageWidth/2, CV_8UC1);
	rEye.create(eyeImageHeight, eyeImageWidth/2, CV_8UC1);
	
	ofxXmlSettings XML;
	XML.loadFile("settings/trackingSettings.xml");
	
	string model_path = ofToDataPath(XML.getValue("model_path", "asm/grayall_asm.model"));
	string cascade_path = ofToDataPath(XML.getValue("cascade_path", "asm/haarcascade_frontalface_alt2.xml"));
	
	readASMModel(asmModel, model_path);
	bool res = faceCascade.load(cascade_path);
	
	for (int i=0; i<numEyePositions; i++) {
		initTemplates[i].create(eyeTemplateSize, eyeTemplateSize, CV_8UC1);
		prevTemplates[i].create(eyeTemplateSize, eyeTemplateSize, CV_8UC1);
		eyePositions[i] = Point2d(0,0);
	}
	
	dispFont.loadFont("fonts/HelveticaNeueMed.ttf", 32);
	
	currentMode = TM_MODE_DETECTION;
	bBeenCalibrated = false;
}

void trackingManager::update(){
	//--- update video/camera input
	IM.update();
	
	//--- eye tracking (on new frames)	
	if(IM.bIsFrameNew){
		grabFrame();
		
		if(currentMode == TM_MODE_DETECTION) detectEyes();
		else trackEyes();
		
		grabEyeImages();
		
		if(currentMode == TM_MODE_TRACKING) {
			IplImage eye_temp = bEye;
			double gaze_temp[2];
			
			if(gazeEstimator.GetMean(&eye_temp, gaze_temp)) {
				gazeHistories.push_back(Point2f(gaze_temp[0], gaze_temp[1]));
				
				int num_history = gazeHistories.size();
				if(num_history > maxGazeHistory)
				{
					gazeHistories.pop_front();
					num_history--;
				}
				
				double w = 1.0/num_history;
				gazePt = w*gazeHistories[0];
				
				for(int i=1; i<num_history; i++)
				{
					gazePt += w*gazeHistories[i];
				}
			}
		}
		
		updateDispImage();
	}
	
	static float prevTime = ofGetElapsedTimef();
	float currTime = ofGetElapsedTimef();
	
	// animate calibration points
	if(currentMode == TM_MODE_CALIBRATION){
		calibPtRad -= round(calibPointsRadSpeed*(currTime - prevTime));
		if(calibPtRad < 0) {
			// add current eye images
			calibrationEyes.push_back(bEye.clone());
			IplImage eye_temp = bEye;
			gazeEstimator.AddExampler(&eye_temp, calibrationPoints[calibPtIdx].x, calibrationPoints[calibPtIdx].y);
			
			calibPtIdx++;
			calibPtRad = calibPointsRadMax;
		}
		if(calibPtIdx == calibPointsNumAll) {
			// start gaze tracker
			gazeEstimator.Update(calibAreaX, calibAreaY);
			bBeenCalibrated = true;
			
			currentMode = TM_MODE_TRACKING;
		}
	}
	
	prevTime = ofGetElapsedTimef();
}

void trackingManager::updateDispImage(){
	Mat dispMat(dispImage.getCvImage());
	Mat greyMat(dispMat.size(), CV_8UC1);
	cvtColor(dispMat, greyMat, CV_BGR2GRAY);
	Mat roi;
	
	// draw eye images		
	roi = greyMat(cv::Rect(0,0,eyeImageWidth,eyeImageHeight));
	bEye.copyTo(roi);
	
	// draw template images if true
	if(true)
	{
		for (int i=0; i<numEyePositions; i++) {
			roi = greyMat(cv::Rect(i*eyeTemplateSize,eyeImageHeight,eyeTemplateSize,eyeTemplateSize));
			initTemplates[i].copyTo(roi);
			roi = greyMat(cv::Rect(i*eyeTemplateSize,eyeImageHeight+eyeTemplateSize,eyeTemplateSize,eyeTemplateSize));
			prevTemplates[i].copyTo(roi);
		}
	}
	
	cvtColor(greyMat, dispMat, CV_GRAY2BGR);
	
	// draw detected eye positions
	for (int i=0; i<numEyePositions; i++) {
		circle(dispMat, eyePositions[i], 5, CV_RGB(255,0,0));
	}
}

void trackingManager::draw(){
	if(currentMode == TM_MODE_CALIBRATION) {
		ofSetColor(255,255,255,50);
		ofCircle(calibrationPoints[calibPtIdx].x, calibrationPoints[calibPtIdx].y, calibPtRad);
		
		ofSetColor(255,255,255);
		static const int lineW = 10;
		ofLine(calibrationPoints[calibPtIdx].x-lineW, calibrationPoints[calibPtIdx].y, 
			   calibrationPoints[calibPtIdx].x+lineW, calibrationPoints[calibPtIdx].y);
		ofLine(calibrationPoints[calibPtIdx].x, calibrationPoints[calibPtIdx].y-lineW, 
			   calibrationPoints[calibPtIdx].x, calibrationPoints[calibPtIdx].y+lineW);
	}
	else {
		dispImage.draw((ofGetWidth()-IM.width)/2, (ofGetHeight()-IM.height)/2, IM.width, IM.height);
		
		if(currentMode == TM_MODE_DETECTION)
			dispFont.drawString("Press the SPACE key to enter tracking mode", 100, 100);
		else if(currentMode == TM_MODE_TRACKING)
			dispFont.drawString("Press the SPACE key again to enter calibration mode", 100, 100);
	}
}

void trackingManager::grabFrame(){
	cvCopy(IM.grayImage->getCvImage(), sourceImage);
	cvCvtColor(sourceImage, colorImage, CV_GRAY2RGB);
	dispImage = colorImage;
}

bool trackingManager::grabEyeImages(){
	for (int i=0; i<numEyePositions; i++) {
		if(eyePositions[i].x == 0) return false;
		if(eyePositions[i].y == 0) return false;
	}
	
	Mat img(sourceImage);
	Mat EyeRaw(eyeImageHeight, eyeImageWidth/2, CV_8UC1);
	double sigmaColor = 20;
	double sigmaSpace = 3;
	
	static const int offset = round(eyeImageWidth/30);
	static const Point2f dst[] = {
		Point2f(offset, 0),
		Point2f(offset, eyeImageHeight/2),
		Point2f(eyeImageWidth/2-offset, eyeImageHeight/2)
	};
	static const double ratio = (double)(eyeImageHeight/2)/(eyeImageWidth/2 - 2*offset);
	
	// left eye
	float left_y = (eyePositions[0].y + eyePositions[1].y)/2;
	Point2f src_left[] = {
		Point2f(eyePositions[0].x, left_y - ratio*norm(eyePositions[1]-eyePositions[0])),
		Point2f(eyePositions[0].x, left_y),
		Point2f(eyePositions[1].x, left_y),
	};
	Mat leftM = getAffineTransform(src_left, dst);
	warpAffine(img, EyeRaw, leftM, lEye.size());
	bilateralFilter(EyeRaw, lEye, -1, sigmaColor, sigmaSpace, BORDER_CONSTANT);
	equalizeHist(lEye, lEye);
	
	// right eye
	float right_y = (eyePositions[2].y + eyePositions[3].y)/2;
	Point2f src_right[] = {
		Point2f(eyePositions[2].x, left_y - ratio*norm(eyePositions[3]-eyePositions[2])),
		Point2f(eyePositions[2].x, left_y),
		Point2f(eyePositions[3].x, left_y),
	};
	Mat rightM = getAffineTransform(src_right, dst);
	warpAffine(img, EyeRaw, rightM, rEye.size());
	bilateralFilter(EyeRaw, rEye, -1, sigmaColor, sigmaSpace, BORDER_CONSTANT);
	equalizeHist(rEye, rEye);
	
	// merge both eyes
	Mat roi;
	roi = bEye(cv::Rect(0,0,eyeImageWidth/2,eyeImageHeight));
	lEye.copyTo(roi);
	roi = bEye(cv::Rect(eyeImageWidth/2,0,eyeImageWidth/2,eyeImageHeight));
	rEye.copyTo(roi);
	
	return true;
}

void trackingManager::readASMModel(ASMModel &asmModel, string modelPath){
    ModelFileAscii mf;
    mf.openFile(modelPath.c_str(),"rb");
    asmModel.loadFromFile(mf);
    mf.closeFile();
}

bool trackingManager::detectEyes(){
	Mat img(sourceImage);
	vector<FitResult> fitResult;
	
	double scale = 0.5;
	Mat small_img;
	resize(img, small_img, cv::Size(0,0), scale, scale);
	
	asmModel.fit(small_img, fitResult, faceCascade, true);
	
	// if detected
	if(fitResult.size() > 0)
	{
		vector< Point_<int> > ptslist;
		asmModel.resultToPointList(fitResult[0], ptslist);
		
		// left-out
		eyeHistories[0].push_back(Point2f(ptslist[27].x/scale, ptslist[27].y/scale));
		// left-in
		eyeHistories[1].push_back(Point2f(ptslist[29].x/scale, ptslist[29].y/scale));
		// right-in
		eyeHistories[2].push_back(Point2f(ptslist[34].x/scale, ptslist[34].y/scale));
		// right-out
		eyeHistories[3].push_back(Point2f(ptslist[32].x/scale, ptslist[32].y/scale));
		
		int num_history = eyeHistories[0].size();
		if(num_history > maxEyeHistory)
		{
			for (int i=0; i<numEyePositions; i++) {
				eyeHistories[i].pop_front();
			}
			num_history--;
		}
		
		double w = 1.0/num_history;
		for (int i=0; i<numEyePositions; i++) {
			eyePositions[i] = w*eyeHistories[i][0];
			
			for(int j=1; j<num_history; j++)
			{
				eyePositions[i] += w*eyeHistories[i][j];
			}
			
			cv::Rect tmpl(eyePositions[i].x-eyeTemplateSize/2, eyePositions[i].y-eyeTemplateSize/2, 
						  eyeTemplateSize, eyeTemplateSize);
			
			if(tmpl.x < 0) tmpl.x = 0;
			else if(tmpl.x > img.size().width-eyeTemplateSize) tmpl.x = img.size().width-eyeTemplateSize;
			if(tmpl.y < 0) tmpl.y = 0;
			else if(tmpl.y > img.size().height-eyeTemplateSize) tmpl.y = img.size().height-eyeTemplateSize;
			
			img(tmpl).copyTo(initTemplates[i]);
			img(tmpl).copyTo(prevTemplates[i]);
		}
		
		return true;
	}
	else return false;
}

bool trackingManager::trackEyes()
{
	for (int i=0; i<numEyePositions; i++) {
		if(eyePositions[i].x < searchWindowSize/2) return false;
		if(eyePositions[i].x > sourceImage->width-searchWindowSize) return false;
		if(eyePositions[i].y < searchWindowSize/2) return false;
		if(eyePositions[i].y > sourceImage->height-searchWindowSize) return false;
	}
	
	Mat img(sourceImage);
	int rw = searchWindowSize - eyeTemplateSize + 1;
	Mat initV(cv::Size(rw,rw), CV_32FC1);
	Mat prevV(cv::Size(rw,rw), CV_32FC1);
	Mat resultV(cv::Size(rw,rw), CV_32FC1);
	double alpha = 1.0; // resultV = initV + alpha*resultV
	
	for (int i=0; i<numEyePositions; i++) {
		cv::Rect roi(eyePositions[i].x-searchWindowSize/2, eyePositions[i].y-searchWindowSize/2, 
					 searchWindowSize, searchWindowSize);
		
		matchTemplate(img(roi), initTemplates[i], initV, CV_TM_CCOEFF_NORMED);
		matchTemplate(img(roi), prevTemplates[i], prevV, CV_TM_CCOEFF_NORMED);
		addWeighted(prevV, alpha, initV, 1.0, 0.0, resultV);
		
		double minVal, maxVal;
		cv::Point minLoc, maxLoc;
		minMaxLoc(resultV, &minVal, &maxVal, &minLoc, &maxLoc);
		
		eyePositions[i].x = roi.x + maxLoc.x + eyeTemplateSize/2;
		eyePositions[i].y = roi.y + maxLoc.y + eyeTemplateSize/2;
		
		cv::Rect tmpl(eyePositions[i].x-eyeTemplateSize/2, eyePositions[i].y-eyeTemplateSize/2, 
					  eyeTemplateSize, eyeTemplateSize);
		
		if(tmpl.x < 0) tmpl.x = 0;
		else if(tmpl.x > img.size().width-eyeTemplateSize) tmpl.x = img.size().width-eyeTemplateSize;
		if(tmpl.y < 0) tmpl.y = 0;
		else if(tmpl.y > img.size().height-eyeTemplateSize) tmpl.y = img.size().height-eyeTemplateSize;
		
		// update prevTemplates
		img(tmpl).copyTo(prevTemplates[i]);
	}
}

void trackingManager::keyPressed(int key) {
	if (key == ' '){
		if(currentMode == TM_MODE_DETECTION) {
			currentMode = TM_MODE_TRACKING;
		}
		else if(currentMode == TM_MODE_TRACKING) {
			currentMode = TM_MODE_CALIBRATION;
			
			calibrationPoints.clear();
			calibAreaX = ofGetWidth();
			calibAreaY = ofGetHeight();
			static const float gridOffset = 10;
			float gridW = (float)(calibAreaX-2*gridOffset)/(calibPointsNumX-1);
			float gridH = (float)(calibAreaY-2*gridOffset)/(calibPointsNumY-1);
			for (int y=0; y<calibPointsNumY; y++) {
				for (int x=0; x<calibPointsNumX; x++) {
					calibrationPoints.push_back(Point2f(gridOffset+x*gridW, gridOffset+y*gridH));
				}
			}
			
			// additional points
			calibrationPoints.push_back(Point2f(gridOffset, gridOffset));
			calibrationPoints.push_back(Point2f(calibAreaX-gridOffset, gridOffset));
			calibrationPoints.push_back(Point2f(gridOffset, calibAreaY-gridOffset));
			calibrationPoints.push_back(Point2f(calibAreaX-gridOffset, calibAreaY-gridOffset));
			calibPointsNumAll = calibrationPoints.size();
			
			calibPtIdx = 0;
			calibPtRad = calibPointsRadMax;
			calibrationEyes.clear();
			gazeEstimator.Reset();
			bBeenCalibrated = false;
		}
		else {
			currentMode = TM_MODE_DETECTION;
		}
	}
}