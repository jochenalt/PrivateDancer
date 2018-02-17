
#include <stdio.h>

#include "basics/logger.h"
#include "basics/util.h"
#include "MoveMaker.h"
#include "WindowController.h"


#include "BotView.h"
#include "uiconfig.h"
#include "setup.h"

using namespace std;

// Initial main window size
int WindowWidth = 600;
int WindowHeight = 1000;

// GLUI Window handlers
int wMain;			// main window
int wMainBotView; 	// sub window with dancing bot

GLUI_RadioGroup* currentDancingModeWidget = NULL;
int dancingModeLiveVar = 0;

GLUI_RadioGroup* currentSequenceModeWidget = NULL;
int currentSequenceModeLiveVar = 0;

// each mouse motion call requires a display() call before doing the next mouse motion call. postDisplayInitiated
// is a semaphore that coordinates this across several threads
// (without that, we have so many motion calls that rendering is bumpy)
// postDisplayInitiated is true, if a display()-invokation is pending but has not yet been executed (i.e. allow a following display call)
volatile static bool postDisplayInitiated = true;


void WindowController::postRedisplay() {
	int saveWindow = glutGetWindow();
	glutSetWindow(wMain);
	postDisplayInitiated = true;
	glutPostRedisplay();
	glutSetWindow(saveWindow );
}


/* Handler for window-repaint event. Call back when the window first appears and
 whenever the window needs to be re-painted. */
void displayMainView() {
	if (!postDisplayInitiated)
		return;

	postDisplayInitiated = false;
	glClear(GL_COLOR_BUFFER_BIT);
}

void nocallback(int value) {
}

void reshape(int w, int h) {
	int savedWindow = glutGetWindow();
	WindowWidth = w;
	WindowHeight = h;
	glViewport(0, 0, w, h);

	int MainSubWindowWidth = w - 2 * WindowGap;
	int MainSubWindowHeight = h - InteractiveWindowHeight - 3 * WindowGap;

	WindowController::getInstance().mainBotView.reshape(WindowGap, WindowGap,MainSubWindowWidth, MainSubWindowHeight);

	glutSetWindow(savedWindow);
}

void GluiReshapeCallback( int x, int y )
{
	reshape(x,y);
	int tx, ty, tw, th;
	int saveWindow = glutGetWindow();
	glutSetWindow(wMain);
	GLUI_Master.get_viewport_area( &tx, &ty, &tw, &th );
	glViewport( tx, ty, tw, th );
	glutSetWindow(saveWindow);
	WindowController::getInstance().postRedisplay();
}



void WindowController::setBodyPose(const Pose& bodyPose) {

	Point eyeLookAt = mainBotView.getEyePosition();
	// Point eyeLookAt (500,0,100);

	// compute the position of the look-at point from the heads perspective:
	//      compute homogenous transformation matrix of head
	// 		compute inverse homogenous matrix for reversing the coord system
	// 	    get look at point from heads perspective by multiplying inverse matrix above with look-at-position
	/*
	HomogeneousMatrix bodyTransformation = HomogeneousMatrix(4,4,
					{ 1, 	0,  	0,  	bodyPose.position.x,
					  0, 	1, 		0,	 	bodyPose.position.y,
					  0,	0,		1,		bodyPose.position.z,
					  0,	0,		0,		1});
	HomogeneousMatrix rotateBody;
	createRotationMatrix(bodyPose.orientation, rotateBody);
	bodyTransformation *= rotateBody;
	HomogeneousMatrix inverseBodyTransformation;
	*/
	HomogeneousMatrix bodyTransformation;
	HomogeneousMatrix inverseBodyTransformation;

	createTransformationMatrix(bodyPose, bodyTransformation);
	computeInverseTransformationMatrix(bodyTransformation, inverseBodyTransformation);

	HomogeneousVector lookAtPosition = {
						eyeLookAt.x,
						eyeLookAt.y,
						eyeLookAt.z,
						1.0 };

	Point lookAtCoordFromBodysPerspective= inverseBodyTransformation * lookAtPosition;
	mainBotView.setBodyPose(bodyPose, lookAtCoordFromBodysPerspective);
}

void setDancingMoveWidget() {
	int moveNumber = (int)MoveMaker::getInstance().getCurrentMove();
	if (moveNumber == Move::MoveType::NO_MOVE)
		currentDancingModeWidget->set_int_val(0);
	else
		currentDancingModeWidget->set_int_val(1+(int)MoveMaker::getInstance().getCurrentMove());
}


void currentDancingMoveCallback(int widgetNo) {
	MoveMaker::getInstance().setCurrentMove((Move::MoveType)(dancingModeLiveVar-1));
}

void setSequenceModeWidget() {
	currentSequenceModeWidget->set_int_val((int)MoveMaker::getInstance().getSequenceMode());
}

void currentSequenceModeCallback(int widgetNo) {
	MoveMaker::getInstance().setSequenceMode((MoveMaker::SequenceModeType)currentSequenceModeLiveVar);
}

GLUI* WindowController::createInteractiveWindow(int mainWindow) {

	string emptyLine = "                                               ";

	GLUI *windowHandle= GLUI_Master.create_glui_subwindow( wMain,  GLUI_SUBWINDOW_BOTTOM);
	windowHandle->set_main_gfx_window( wMain );

	GLUI_Panel* interactivePanel = new GLUI_Panel(windowHandle,"interactive panel", GLUI_PANEL_NONE);

	GLUI_Panel* dancingModePanel = new GLUI_Panel(interactivePanel,"Dancing Mode Panel", GLUI_PANEL_RAISED);
	GLUI_StaticText* text = new GLUI_StaticText(dancingModePanel,"Current Dance Move");
	text->set_alignment(GLUI_ALIGN_LEFT);

	currentDancingModeWidget =  new GLUI_RadioGroup(dancingModePanel, &dancingModeLiveVar, 0, currentDancingMoveCallback);

	new GLUI_RadioButton(currentDancingModeWidget, Move::getMove(Move::NO_MOVE).getName().c_str());
	for (int i = 0;i<MoveMaker::getInstance().getNumMoves();i++) {
		Move& move = Move::getMove((Move::MoveType)i);
		new GLUI_RadioButton(currentDancingModeWidget, move.getName().c_str());

		if (i >= MoveMaker::getInstance().getNumMoves()/2 )
			windowHandle->add_column_to_panel(dancingModePanel, false);
	}

	windowHandle->add_column_to_panel(interactivePanel, false);

	GLUI_Panel* sequenceModePanel= new GLUI_Panel(interactivePanel,"Sequence Mode", GLUI_PANEL_RAISED);
	text = new GLUI_StaticText(sequenceModePanel,"Sequence Move");
	text->set_alignment(GLUI_ALIGN_LEFT);

	currentSequenceModeWidget =  new GLUI_RadioGroup(sequenceModePanel, &currentSequenceModeLiveVar, 0, currentSequenceModeCallback);
	new GLUI_RadioButton(currentSequenceModeWidget, "Automated Sequence");
	new GLUI_RadioButton(currentSequenceModeWidget, "Selected Dance Move");

	return windowHandle;
}


bool WindowController::setup(int argc, char** argv) {
	glutInit(&argc, argv);

	// start the initialization in a thread so that this function returns
	// (the thread runs the endless GLUT main loop)
	// main thread can do something else while the UI is running
	eventLoopThread = new std::thread(&WindowController::UIeventLoop, this);

	// wait until UI is ready
	unsigned long startTime  = millis();
	do { delay_ms(10); }
	while ((millis() - startTime < 20000) && (!uiReady));

	return uiReady;
}


// Idle callback is called by GLUI when nothing is to do.
void idleCallback( void )
{
	const milliseconds emergencyRefreshRate = 1000; 		// refresh everything once a second at least due to refresh issues


	milliseconds now = millis();
	static milliseconds lastDisplayRefreshCall = millis();

	// update all screens once a second in case of refresh issues (happens)
	if ((now - lastDisplayRefreshCall > emergencyRefreshRate)) {
		WindowController::getInstance().mainBotView.postRedisplay();

		setDancingMoveWidget();
	}
}


void WindowController::UIeventLoop() {
	LOG(DEBUG) << "BotWindowCtrl::UIeventLoop";

	glutInitWindowSize(WindowWidth, WindowHeight);
    wMain = glutCreateWindow("Private Dancer"); // Create a window with the given title
	glutInitWindowPosition(20, 20); // Position the window's initial top-left corner
	glutDisplayFunc(displayMainView);
	glutReshapeFunc(reshape);

	GLUI_Master.set_glutReshapeFunc( GluiReshapeCallback );
	GLUI_Master.set_glutIdleFunc( idleCallback);

	wMainBotView= mainBotView.create(wMain,"");

	// Main Bot view has comprehensive mouse motion
	glutSetWindow(wMainBotView);

	// double buffering
	glutInitDisplayMode(GLUT_DOUBLE);

	// initialize all widgets
	createInteractiveWindow(wMain);

	uiReady = true; 							// flag to tell calling thread to stop waiting for ui initialization
	LOG(DEBUG) << "starting GLUT main loop";
	glutMainLoop();
}
