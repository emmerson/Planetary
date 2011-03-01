#include "cinder/app/AppCocoaTouch.h"
#include "cinder/app/Renderer.h"
#include "cinder/Surface.h"
#include "cinder/gl/Texture.h"
#include "cinder/Camera.h"
#include "cinder/Font.h"
#include "cinder/Arcball.h"
#include "cinder/ImageIo.h"
#include "Globals.h"
#include "Easing.h"
#include "World.h"
#include "UiLayer.h"
#include "State.h"
#include "Data.h"
#include "Breadcrumbs.h"
#include "BreadcrumbEvent.h"
#include <vector>

using std::vector;
using namespace ci;
using namespace ci::app;
using namespace std;

int G_CURRENT_LEVEL	= 0;

float easeInOutQuad( double t, float b, float c, double d );
Vec3f easeInOutQuad( double t, Vec3f b, Vec3f c, double d );

class KeplerApp : public AppCocoaTouch {
  public:
	virtual void	setup();
	virtual void	touchesBegan( TouchEvent event );
	virtual void	touchesMoved( TouchEvent event );
	virtual void	touchesEnded( TouchEvent event );
	void			initFonts();
	virtual void	update();
	void			updateArcball();
	void			updateCamera();
	virtual void	draw();
	bool			onAlphaCharStateChanged( State *state );
	bool			onAlphaCharSelected( UiLayer *uiLayer );
	bool			onBreadcrumbSelected ( BreadcrumbEvent event );
	bool			onNodeSelected( Node *node );
	void			checkForNodeTouch( const Ray &ray, Matrix44f &mat );
	World			mWorld;
	State			mState;
	UiLayer			mUiLayer;
	Data			mData;
	
	// BREADCRUMBS
	Breadcrumbs		mBreadcrumbs;	
	
	// CAMERA PERSP
	CameraPersp		mCam;
	float			mFov, mFovDest;
	Vec3f			mEye, mCenter, mUp;
	Vec3f			mCamVel;
	Vec3f			mCenterDest, mCenterFrom;
	float			mCamDist, mCamDistDest, mCamDistFrom, mCamDistDestMulti;
	float			mZoomFrom, mZoomDest;
	Arcball			mArcball;
	Matrix44f		mMatrix;
	Vec3f			mBbRight, mBbUp;
	
	
	// FONTS
	vector<Font>	mFonts;
	
	
	// MULTITOUCH
	Vec2f			mTouchPos;
	Vec2f			mTouchThrowVel;
	Vec2f			mTouchVel;
	bool			mIsDragging;
	
	
	// TEXTURES
	gl::Texture		mStarTex;
	gl::Texture		mStarGlowTex;
	
	float			mTime;
};

void KeplerApp::setup()
{
	// ARCBALL
	mMatrix	= Quatf();
	mArcball.setWindowSize( getWindowSize() );
	mArcball.setCenter( getWindowCenter() );
	mArcball.setRadius( 300 );
	
	
	// CAMERA PERSP
	mCamDist			= 180.0f;
	mCamDistDest		= mCamDist;
	mCamDistFrom		= mCamDist;
	mCamDistDestMulti	= 1.0f;
	mEye				= Vec3f( 0.0f, 0.0f, mCamDist );
	mCenter				= Vec3f::zero();
	mCenterDest			= mCenter;
	mCenterFrom			= mCenter;
	mUp					= Vec3f::yAxis();
	mFov				= 90.0f;
	mFovDest			= 90.0f;
	mCam.setPerspective( mFov, getWindowAspectRatio(), 0.001f, 4000.0f );
	mBbRight			= Vec3f::xAxis();
	mBbUp				= Vec3f::yAxis();
	
	
	// FONTS
	initFonts();
	
	
	// TOUCH VARS
	mTouchPos			= Vec2f::zero();
	mTouchThrowVel		= Vec2f::zero();
	mTouchVel			= Vec2f::zero();
	mIsDragging			= false;
	mTime				= getElapsedSeconds();
	
	
	// TEXTURES
	mStarTex			= gl::Texture( loadImage( loadResource( "star.png" ) ) );
	mStarGlowTex		= gl::Texture( loadImage( loadResource( "starGlow.png" ) ) );
	
	
	// BREADCRUMBS
	mBreadcrumbs.setup( this, mFonts[4] );
	mBreadcrumbs.registerBreadcrumbSelected( this, &KeplerApp::onBreadcrumbSelected );
	mBreadcrumbs.setHierarchy(mState.getHierarchy());
	
	
	// STATE
	mState.registerAlphaCharStateChanged( this, &KeplerApp::onAlphaCharStateChanged );
	mState.registerNodeSelected( this, &KeplerApp::onNodeSelected );
	
	
	// UILAYER
	mUiLayer.setup( this );
	mUiLayer.registerAlphaCharSelected( this, &KeplerApp::onAlphaCharSelected );
	mUiLayer.initAlphaTextures( mFonts[0] );
	
	
	// DATA
	mData.initArtists();
	

	// WORLD
	mWorld.setData( &mData );
	mWorld.initNodes( mFonts[3] );
}

void KeplerApp::touchesBegan( TouchEvent event )
{	
	mIsDragging = false;
	for( vector<TouchEvent::Touch>::const_iterator touchIt = event.getTouches().begin(); touchIt != event.getTouches().end(); ++touchIt ) 
	{
		mTouchPos		= touchIt->getPos();
		mTouchThrowVel	= Vec2f::zero();
		mIsDragging		= false;
		if( event.getTouches().size() == 1 )
			mArcball.mouseDown( Vec2f( mTouchPos.x, mTouchPos.y ) );
	}
}

void KeplerApp::touchesMoved( TouchEvent event )
{
	mIsDragging = true;
	for( vector<TouchEvent::Touch>::const_iterator touchIt = event.getTouches().begin(); touchIt != event.getTouches().end(); ++touchIt )
	{
		mTouchThrowVel	= touchIt->getPos() - mTouchPos;
		mTouchVel		= mTouchThrowVel;
		mTouchPos		= touchIt->getPos();
		if( event.getTouches().size() == 1 )
			mArcball.mouseDrag( Vec2f( mTouchPos.x, mTouchPos.y ) );
	}
}

void KeplerApp::touchesEnded( TouchEvent event )
{
	for( vector<TouchEvent::Touch>::const_iterator touchIt = event.getTouches().begin(); touchIt != event.getTouches().end(); ++touchIt )
	{
		mTouchPos = touchIt->getPos();
		if( ! mUiLayer.getShowWheel() && ! mIsDragging ){
			float u			= mTouchPos.x / (float) getWindowWidth();
			float v			= mTouchPos.y / (float) getWindowHeight();
			Ray touchRay	= mCam.generateRay( u, 1.0f - v, mCam.getAspectRatio() );
			checkForNodeTouch( touchRay, mMatrix );
		}
		mIsDragging = false;
	}
}

void KeplerApp::initFonts()
{
	mFonts.push_back( Font( loadResource( "UnitRoundedOT-Ultra.otf" ), 256 ) );
	mFonts.push_back( Font( loadResource( "UnitRoundedOT-Ultra.otf" ), 64 ) );
	mFonts.push_back( Font( loadResource( "UnitRoundedOT-Ultra.otf" ), 48 ) );
	mFonts.push_back( Font( loadResource( "UnitRoundedOT-Ultra.otf" ), 32 ) );
	mFonts.push_back( Font( loadResource( "UnitRoundedOT-Ultra.otf" ), 24 ) );
}

bool KeplerApp::onAlphaCharSelected( UiLayer *uiLayer )
{
	mState.setAlphaChar( uiLayer->getAlphaChar() );
	return false;
}

bool KeplerApp::onAlphaCharStateChanged( State *state )
{
	mData.filterArtistsByAlpha( mState.getAlphaChar() );
	mWorld.filterNodes();
	mBreadcrumbs.setHierarchy( mState.getHierarchy() );	
	return false;
}

bool KeplerApp::onNodeSelected( Node *node )
{
	mTime			= getElapsedSeconds();
	mCenterFrom		= mCenter;
	mCamDistFrom	= mCamDist;				
	mBreadcrumbs.setHierarchy( mState.getHierarchy() );	
	return false;
}

bool KeplerApp::onBreadcrumbSelected( BreadcrumbEvent event )
{
	int level = event.getLevel();
	if( level == 0 ){					// BACK TO HOME
		mWorld.deselectAllNodes();
		mState.setSelectedNode(NULL);
	}
	else if( level == 1 ){			// BACK TO ALPHA FILTER
		mWorld.deselectAllNodes();
		mData.filterArtistsByAlpha( mState.getAlphaChar() );
		mWorld.filterNodes();
		mState.setSelectedNode(NULL);
	} else {
		// get Artist
		// get Album
		// get Track
	}
	return false;
}

void KeplerApp::checkForNodeTouch( const Ray &ray, Matrix44f &mat )
{
	Node *touchedNode = NULL;
	mWorld.checkForSphereIntersect( touchedNode, ray, mat );
	
	// TODO: is it actually sensible to setSelectedNode( NULL )?
	if( touchedNode ) mState.setSelectedNode(touchedNode);
}

void KeplerApp::update()
{
	updateArcball();
	updateCamera();
	mWorld.update( mMatrix, mBbRight, mBbUp );
	mBreadcrumbs.update();
}

void KeplerApp::updateArcball()
{
	if( mTouchThrowVel.length() > 10.0f && !mIsDragging ){
		if( mTouchVel.length() > 1.0f ){
			mTouchVel *= 0.99f;
			mArcball.mouseDown( mTouchPos );
			mArcball.mouseDrag( mTouchPos + mTouchVel );
		}
	}
	
	mMatrix = mArcball.getQuat();
}


void KeplerApp::updateCamera()
{
	if( mState.getSelectedNode() ){
		
		Node* selectedNode = mState.getSelectedNode();
		
		float radiusMulti = 15.0f;
		
		mCamDistDest	= ( selectedNode->mRadius * radiusMulti  );
		mCenterDest		= mMatrix.transformPointAffine( selectedNode->mPos );
		mZoomDest		= selectedNode->mGen;
		
		if( selectedNode->mParentNode )
			mCenterFrom		+= selectedNode->mParentNode->mVel;

	}
	
	
	
	// UPDATE FOV
	if( mUiLayer.getShowWheel() ){
		mFovDest = 130.0f;
	} else {
		mFovDest = 90.0f;
	}
	mFov -= ( mFov - mFovDest ) * 0.2f;
	
	double p	= constrain( getElapsedSeconds()-mTime, 0.0, G_DURATION );
	mCenter		= easeInOutQuad( p, mCenterFrom, mCenterDest - mCenterFrom, G_DURATION );
	mCamDist	= easeInOutQuad( p, mCamDistFrom, mCamDistDest*mCamDistDestMulti - mCamDistFrom, G_DURATION );
	
	Vec3f prevEye		= mEye;
	mEye				= Vec3f( mCenter.x, mCenter.y, mCenter.z - mCamDist );
	mCamVel				= mEye - prevEye;
	
	mCam.setPerspective( mFov, getWindowAspectRatio(), 0.01f, 4000.0f );
	mCam.lookAt( mEye, mCenter, mUp );
	mCam.getBillboardVectors( &mBbRight, &mBbUp );
}

void KeplerApp::draw()
{
	gl::clear( Color( 0, 0, 0 ), true );
	gl::enableDepthWrite();
	gl::setMatrices( mCam );
	
	gl::enableAdditiveBlending();
	
	mStarGlowTex.enableAndBind();
	mWorld.drawStarGlows();
	mStarGlowTex.disable();
	
	mStarTex.enableAndBind();
	mWorld.drawStars();
	mStarTex.disable();
	
	mWorld.drawNames();
	glDisable( GL_TEXTURE_2D );
	
	gl::disableAlphaBlending();
	
	gl::disableDepthWrite();
	gl::enableAlphaBlending();
	gl::setMatricesWindow( getWindowSize() );
	mUiLayer.draw();
	mBreadcrumbs.draw();
}


CINDER_APP_COCOA_TOUCH( KeplerApp, RendererGl )
