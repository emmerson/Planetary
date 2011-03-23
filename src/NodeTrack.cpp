/*
 *  NodeTrack.cpp
 *  Bloom
 *
 *  Created by Robert Hodgin on 1/21/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "cinder/app/AppBasic.h"
#include "NodeTrack.h"
#include "cinder/Text.h"
#include "cinder/Rand.h"
#include "cinder/gl/gl.h"
#include "Globals.h"

using namespace ci;
using namespace ci::ipod;
using namespace std;

NodeTrack::NodeTrack( Node *parent, int index, const Font &font )
	: Node( parent, index, font )
{	
	mIsHighlighted		= true;
	mRadius				*= 10.0f;
    mIsPlaying			= false;
	mHasClouds			= false;
	mIsPopulated		= false;
}

void NodeTrack::setData( TrackRef track, PlaylistRef album )
{
	mAlbum			= album;
	mTrack			= track;
	mTrackLength	= (*mAlbum)[mIndex]->getLength();
	mPlayCount		= (*mAlbum)[mIndex]->getPlayCount();
	mStarRating		= (*mAlbum)[mIndex]->getStarRating();
	
	/*
	mAlbumArt		= mTrack->getArtwork( Vec2i( 256, 256 ) );
	if( !mAlbumArt ){
		mAlbumArt	= gl::Texture( 256, 256 );
	}
	*/
	
	mOrbitPath.clear();
	
	
	//normalize playcount data
	float playCountDelta	= ( mParentNode->mHighestPlayCount - mParentNode->mLowestPlayCount ) + 1.0f;
	float normPlayCount		= ( mPlayCount - mParentNode->mLowestPlayCount )/playCountDelta;
    
	// TODO: try making own texture for ringed planet. texture stripe, maybe from album art?
	mPlanetTexIndex = mIndex%( G_NUM_PLANET_TYPES * G_NUM_PLANET_TYPE_OPTIONS );//(int)( normPlayCount * ( G_NUM_PLANET_TYPES - 1 ) );
	mCloudTexIndex  = Rand::randInt( G_NUM_CLOUD_TYPES );
   // mPlanetTexIndex *= G_NUM_PLANET_TYPE_OPTIONS + Rand::randInt( G_NUM_PLANET_TYPE_OPTIONS );
	
	if( mPlayCount > 50 ){
		mCloudTexIndex = 2;
	} else if( mPlayCount > 10 ){
		mCloudTexIndex = 1;
	} else {
		mCloudTexIndex = 0;
	}
	
	if( mPlayCount > 5 ){
		mHasClouds = true;
	}
	
	
	float hue			= Rand::randFloat( 0.15f, 0.75f );
	float sat			= Rand::randFloat( 0.15f, 0.5f );
	float val			= Rand::randFloat( 0.85f, 1.00f );
	mColor				= Color( CM_HSV, hue, sat, val );
	mGlowColor			= mColor;
	mEclipseColor		= mColor;
	
	mRadius				= math<float>::max( mRadius * pow( normPlayCount + 0.5f, 2.0f ), 0.0003f ) * 0.75;
	mSphere				= Sphere( mPos, mRadius * 7.5f );
	mMass				= ( pow( mRadius, 3.0f ) * ( M_PI * 4.0f ) ) * 0.3333334f;
	
	mIdealCameraDist	= 0.01f;
	mOrbitPeriod		= mTrackLength;
	mAxialTilt			= Rand::randFloat( 5.0f, 30.0f );
    mAxialVel			= Rand::randFloat( 10.0f, 45.0f );

	mStartRelPos		= Vec3f( cos( mOrbitStartAngle ), sin( mOrbitStartAngle ), 0.0f ) * mOrbitRadius;
	mStartPos			= ( mParentNode->mPos + mStartRelPos ); 
}

void NodeTrack::updateAudioData( double currentPlayheadTime )
{
	if( mIsPlaying ){
		mPercentPlayed		= currentPlayheadTime/mTrackLength;
		mOrbitAngle			= mPercentPlayed * TWO_PI + mOrbitStartAngle;
		
		// TODO: Find a better way to do this without clearing mOrbitPath every frame.
		mOrbitPath.clear();
		
		// Add start position
		mOrbitPath.push_back( mStartRelPos );
		
		// Add middle positions
		int maxNumVecs		= 300;
		int currentNumVecs	= mPercentPlayed * maxNumVecs;
		for( int i=0; i<currentNumVecs; i++ ){
			float per = (float)(i+1)/(float)(maxNumVecs);
			float angle = mOrbitStartAngle + per * TWO_PI;
			Vec2f pos = Vec2f( cos( angle ), sin( angle ) ) * mOrbitRadius;
			
			mOrbitPath.push_back( Vec3f( pos.x, pos.y, 0.0f ) );
		}
		
		// Add end position
		mOrbitPath.push_back( mRelPos );
	}
}

void NodeTrack::update( const Matrix44f &mat )
{	
	if( !mIsPlaying ){
		// TODO: THIS IS AWKWARD
		mPercentPlayed	= app::getElapsedSeconds()/mOrbitPeriod;
		mOrbitAngle		= mPercentPlayed * TWO_PI + mOrbitStartAngle;
	}
	
	
	mPrevPos			= mTransPos;
	mRelPos				= Vec3f( cos( mOrbitAngle ), sin( mOrbitAngle ), 0.0f ) * mOrbitRadius;
	mPos				= mParentNode->mPos + mRelPos;
	
	if( mIsPlaying ){
		mStartRelPos		= Vec3f( cos( mOrbitStartAngle ), sin( mOrbitStartAngle ), 0.0f ) * mOrbitRadius;
		mTransStartPos		= mat * ( mParentNode->mPos + mStartRelPos );
	}
	
    float eclipseDist	= 1.0f;
    if( mParentNode->mParentNode->mDistFromCamZAxisPer > 0.0f ){
        float dist = mScreenPos.distance( mParentNode->mParentNode->mScreenPos );
        eclipseDist = constrain( dist/200.0f, 0.0f, 1.0f );
		if( G_ZOOM == G_TRACK_LEVEL ){
			mEclipseStrength	= math<float>::max( 250.0f - abs( mSphereScreenRadius - mParentNode->mParentNode->mSphereScreenRadius ), 0.0f ) / 250.0f; 
			mEclipseStrength	= pow( mEclipseStrength, 5.0f );
		}
	}
	mEclipseColor = ( mColor + Color::white() ) * 0.5f * eclipseDist;
	
	
	Node::update( mat );
	
	mVel		= mTransPos - mPrevPos;	
}


void NodeTrack::drawEclipseGlow()
{
	if( mIsSelected && mDistFromCamZAxisPer > 0.0f ){
        gl::color( ColorA( mParentNode->mParentNode->mGlowColor, mEclipseStrength * 3.0f ) );
		Vec2f radius = Vec2f( mRadius, mRadius ) * 3.25f;
		gl::drawBillboard( mTransPos, radius, 0.0f, mBbRight, mBbUp );
	}
}


void NodeTrack::drawPlanet( const vector<gl::Texture> &planets )
{	
	glEnable( GL_RESCALE_NORMAL );
	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glEnableClientState( GL_NORMAL_ARRAY );
	int numVerts;
	if( mIsSelected ){
		glVertexPointer( 3, GL_FLOAT, 0, mSphereVertsHiRes );
		glTexCoordPointer( 2, GL_FLOAT, 0, mSphereTexCoordsHiRes );
		glNormalPointer( GL_FLOAT, 0, mSphereNormalsHiRes );
		numVerts = mTotalVertsHiRes;
	} else {
		glVertexPointer( 3, GL_FLOAT, 0, mSphereVertsLoRes );
		glTexCoordPointer( 2, GL_FLOAT, 0, mSphereTexCoordsLoRes );
		glNormalPointer( GL_FLOAT, 0, mSphereNormalsLoRes );
		numVerts = mTotalVertsLoRes;
	}
	
	gl::disableAlphaBlending();
	
    gl::pushModelView();
	gl::translate( mTransPos );
	gl::scale( Vec3f( mRadius, mRadius, mRadius ) );
	gl::rotate( mMatrix );
	gl::rotate( Vec3f( 0.0f, 0.0f, mAxialTilt ) );
	gl::rotate( Vec3f( 90.0f, app::getElapsedSeconds() * mAxialVel, 0.0f ) );
	gl::color( mEclipseColor );
	planets[mPlanetTexIndex].enableAndBind();
	glDrawArrays( GL_TRIANGLES, 0, numVerts );
	gl::popModelView();
	
	glDisableClientState( GL_VERTEX_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	glDisableClientState( GL_NORMAL_ARRAY );
	
}

void NodeTrack::drawClouds( const vector<gl::Texture> &clouds )
{
	if( mCamDistAlpha > 0.05f && mHasClouds ){
		glEnableClientState( GL_VERTEX_ARRAY );
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		glEnableClientState( GL_NORMAL_ARRAY );
		int numVerts;
		if( mIsSelected ){
			glVertexPointer( 3, GL_FLOAT, 0, mSphereVertsHiRes );
			glTexCoordPointer( 2, GL_FLOAT, 0, mSphereTexCoordsHiRes );
			glNormalPointer( GL_FLOAT, 0, mSphereNormalsHiRes );
			numVerts = mTotalVertsHiRes;
		} else {
			glVertexPointer( 3, GL_FLOAT, 0, mSphereVertsLoRes );
			glTexCoordPointer( 2, GL_FLOAT, 0, mSphereTexCoordsLoRes );
			glNormalPointer( GL_FLOAT, 0, mSphereNormalsLoRes );
			numVerts = mTotalVertsLoRes;
		}
		
		gl::disableAlphaBlending();
		
		gl::pushModelView();
		gl::translate( mTransPos );
		gl::pushModelView();
		float radius = mRadius + 0.000025f;
		gl::scale( Vec3f( radius, radius, radius ) );
		glEnable( GL_RESCALE_NORMAL );
		gl::rotate( mMatrix );
		gl::rotate( Vec3f( 0.0f, 0.0f, mAxialTilt ) );
		gl::rotate( Vec3f( 90.0f, app::getElapsedSeconds() * mAxialVel * 0.6f, 0.0f ) );

		// SHADOW CLOUDS
		glDisable( GL_LIGHTING );
		gl::disableAlphaBlending();
		gl::enableAlphaBlending();
		gl::color( ColorA( 0.0f, 0.0f, 0.0f, mCamDistAlpha * 0.5f ) );
		clouds[mCloudTexIndex].enableAndBind();
		glDrawArrays( GL_TRIANGLES, 0, numVerts );
		gl::popModelView();
		glEnable( GL_LIGHTING );
		// LIT CLOUDS
		gl::pushModelView();
		radius = mRadius + 0.00005f;
		gl::scale( Vec3f( radius, radius, radius ) );
		glEnable( GL_RESCALE_NORMAL );
		gl::rotate( mMatrix );
		gl::rotate( Vec3f( 0.0f, 0.0f, mAxialTilt ) );
		gl::rotate( Vec3f( 90.0f, app::getElapsedSeconds() * mAxialVel * 0.6f, 0.0f ) );
		gl::enableAdditiveBlending();
		gl::color( ColorA( mEclipseColor, mCamDistAlpha ) );
		glDrawArrays( GL_TRIANGLES, 0, numVerts );
		gl::popModelView();
		gl::popModelView();
		
		glDisableClientState( GL_VERTEX_ARRAY );
		glDisableClientState( GL_TEXTURE_COORD_ARRAY );
		glDisableClientState( GL_NORMAL_ARRAY );

	}
}


void NodeTrack::drawOrbitRing( float pinchAlphaOffset, GLfloat *ringVertsLowRes, GLfloat *ringVertsHighRes )
{
	if( mIsPlaying ){
		gl::color( ColorA( 0.2f, 0.3f, 0.7f, 0.45f * pinchAlphaOffset ) );
	} else {
		gl::color( ColorA( 0.15f, 0.2f, 0.4f, 0.2f * pinchAlphaOffset  ) );
	}
	gl::pushModelView();
	gl::translate( mParentNode->mTransPos );
	gl::scale( Vec3f( mOrbitRadius, mOrbitRadius, mOrbitRadius ) );
	gl::rotate( mMatrix );
	glEnableClientState( GL_VERTEX_ARRAY );
	glVertexPointer( 2, GL_FLOAT, 0, ringVertsLowRes );
	glDrawArrays( GL_LINE_STRIP, 0, G_RING_LOW_RES );
	glDisableClientState( GL_VERTEX_ARRAY );
	gl::popModelView();
	
	drawPlayheadProgress();
}

void NodeTrack::drawPlayheadProgress()
{
	if( mIsPlaying ){
		gl::pushModelView();
		gl::translate( mParentNode->mTransPos );
		gl::rotate( mMatrix );
		
		gl::color( Color::white() );
		PolyLine<Vec3f> mLine;
		vector<Vec3f>::iterator it;
		for( it = mOrbitPath.begin(); it != mOrbitPath.end(); ++it ){
			mLine.push_back( *it );
		}
		gl::draw( mLine );
		
		/*
		gl::color( ColorA( 0.0f, 1.0f, 0.0f, 1.0f ) );
		gl::drawLine( Vec3f::zero(), mRelPos );
		
		gl::color( ColorA( 1.0f, 0.0f, 0.0f, 1.0f ) );
		gl::drawLine( Vec3f::zero(), mStartRelPos );
		 */
		
		gl::color( mColor );
		gl::drawSphere( mStartRelPos, 0.0005f );
		
		gl::popModelView();
	}
}

string NodeTrack::getName()
{
	string name = mTrack->getTitle();
	if( name.size() < 1 ) name = "Untitled";
	return name;
}

uint64_t NodeTrack::getId()
{
    return mTrack->getItemId();
}
