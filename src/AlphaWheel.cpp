/*
 *  AlphaWheel.cpp
 *  Kepler
 *
 *  Created by Tom Carden on 3/14/11.
 *  Copyright 2011 __MyCompanyName__. All rights reserved.
 *
 */

#include "AlphaWheel.h"
#include "cinder/gl/gl.h"
#include "cinder/Font.h"
#include "cinder/Text.h"
#include "cinder/ImageIo.h"
#include "Globals.h"
#include "BloomGl.h"
#include <sstream>

using std::stringstream;
using namespace ci;
using namespace ci::app;
using namespace std;

AlphaWheel::AlphaWheel()
{
}

AlphaWheel::~AlphaWheel()
{
	mApp->unregisterTouchesBegan( mCbTouchesBegan );
	mApp->unregisterTouchesMoved( mCbTouchesMoved );
	mApp->unregisterTouchesEnded( mCbTouchesEnded );
}

void AlphaWheel::setup( AppCocoaTouch *app, const Orientation &orientation, const Font &font )
{
	mApp = app;
	
	mCbTouchesBegan = mApp->registerTouchesBegan( this, &AlphaWheel::touchesBegan );
	mCbTouchesMoved = mApp->registerTouchesMoved( this, &AlphaWheel::touchesMoved );
	mCbTouchesEnded = mApp->registerTouchesEnded( this, &AlphaWheel::touchesEnded );
	
	mAlphaString	= "ABCDEFGHIJKLMNOPQRSTUVWXYZ#";
	mAlphaIndex		= 0;
	mAlphaChar		= ' ';
	mPrevAlphaChar	= ' ';
	mShowWheel		= false;
	mWheelScale		= 1.0f;
	
	mWheelOverlay.setup();
	
	initAlphaTextures( font );
    // just do orientation stuff in here:
    setInterfaceOrientation(orientation);
}

void AlphaWheel::initAlphaTextures( const Font &font )
{
	for( int i=0; i<mAlphaString.length(); i++ ){
		TextLayout layout;	
		layout.setFont( font );
		layout.setColor( ColorA( BRIGHT_BLUE, 1.0f ) );
		stringstream s;
		s.str("");
		s << mAlphaString[i];
		layout.addCenteredLine( s.str() );
		mAlphaTextures.push_back( gl::Texture( layout.render( true, false ) ) );
	}
}

void AlphaWheel::setRects()
{
	float yOff = 0.0f;
	if( isLandscapeOrientation( mInterfaceOrientation ) ){
		yOff = -12.0f;
	}
	
	mAlphaRects.clear();
	for( int i=0; i<mAlphaString.length(); i++ ){
		float per = (float)i/27.0f;
		float angle = per * TWO_PI - M_PI_2;

		float w = mAlphaTextures[i].getWidth()/2.0f;
		float h = mAlphaTextures[i].getHeight()/2.0f;
		Vec2f pos = Vec2f( cos( angle ), sin( angle ) ) * ( mWheelOverlay.mRadius - 10.0f );
		pos.y += yOff;
		Rectf r = Rectf( pos.x - w, pos.y - h, pos.x + w, pos.y + h );
		mAlphaRects.push_back( r );
	}
}

void AlphaWheel::setInterfaceOrientation( const Orientation &orientation )
{
    mInterfaceOrientation = orientation;
    mOrientationMatrix	= getOrientationMatrix44( mInterfaceOrientation, getWindowSize() );
    mInterfaceSize = getWindowSize();
    
    if ( isLandscapeOrientation( mInterfaceOrientation ) ) {
        mInterfaceSize = mInterfaceSize.yx(); // swizzle it!
    }
    
    mInterfaceCenter = mInterfaceSize * 0.5f;
	mWheelOverlay.update( mInterfaceSize );
	setRects();
}

bool AlphaWheel::touchesBegan( TouchEvent event )
{
    if (!mShowWheel) return false;
    
	vector<TouchEvent::Touch> touches = event.getTouches();

    for (int i = 0; i < touches.size(); i++) {
        TouchEvent::Touch touch = touches[i];
        if (selectWheelItem( touch.getPos(), false )) {
            // this means we'll follow the last touch that starts on a wheel item
            mLastTouchPos = touch.getPos();
            mActiveTouchId = touch.getId();
            return true;
        }
    }
	
	return false;
}

bool AlphaWheel::touchesMoved( TouchEvent event )
{
    if (!mShowWheel) return false;
    
	vector<TouchEvent::Touch> touches = event.getTouches();
    
    for (int i = 0; i < touches.size(); i++) {
        TouchEvent::Touch touch = touches[i];
        // only follow the last valid touch we received
        if (touch.getId() == mActiveTouchId) {
            mLastTouchPos = touch.getPos();
            return selectWheelItem( mLastTouchPos, false );
        }
    }
	
	return false;
}

bool AlphaWheel::touchesEnded( TouchEvent event )
{	
    if (!mShowWheel) return false;
    
	vector<TouchEvent::Touch> touches = event.getTouches();

    for (int i = 0; i < touches.size(); i++) {
        TouchEvent::Touch touch = touches[i];
        // only accept the last valid touch we received
        if (touch.getId() == mActiveTouchId) {
            mLastTouchPos = touch.getPos();
            return selectWheelItem( mLastTouchPos, true );
        }
    }
    
	return false;
}


bool AlphaWheel::selectWheelItem( const Vec2f &pos, bool closeWheel )
{
	if( !mShowWheel ){
		return false;
	}
	float minDiam = mWheelOverlay.mRadius - 25.0f;
	float maxDiam = mWheelOverlay.mRadius + 25.0f;
	
	float timeSincePinchEnded = getElapsedSeconds() - mTimePinchEnded;
	if( mShowWheel && timeSincePinchEnded > 0.5f ){ 
        Vec2f dir = (mOrientationMatrix.inverted() * Vec3f(pos,0)).xy() - mInterfaceCenter;
		float distToCenter = dir.length();
		if( distToCenter > minDiam && distToCenter < maxDiam ){
			float touchAngle	= atan2( dir.y, dir.x ) + M_PI;				// RANGE 0 -> TWO_PI
			float anglePer		= ( touchAngle + 0.11365f + M_PI*1.5f )/TWO_PI;
			mAlphaIndex			= (int)( anglePer * 27 )%27;
			mPrevAlphaChar		= mAlphaChar;
			mAlphaChar			= mAlphaString.at( mAlphaIndex % mAlphaString.size() );
			if( mPrevAlphaChar != mAlphaChar || closeWheel ){
				mCallbacksAlphaCharSelected.call( this );
			}
		}
		
		if( closeWheel && distToCenter < maxDiam ){
            setShowWheel(false);
		}

        return distToCenter > minDiam && distToCenter < maxDiam;
	}
    return false;
}

void AlphaWheel::setTimePinchEnded( float timePinchEnded )
{
	mTimePinchEnded = timePinchEnded;	
}

void AlphaWheel::update( float fov )
{
	//mWheelScale = ( ( 130.0f - fov ) / 30.0f );
	if( getShowWheel() ){
		mWheelScale -= ( mWheelScale - 1.0f ) * 0.2f;
	} else {
		mWheelScale -= ( mWheelScale - 2.15f ) * 0.2f;	
	}	
}

void AlphaWheel::draw( float *numberAlphaPerChar )
{	
	if( mWheelScale < 1.95f ){
		glPushMatrix();
		glMultMatrixf( mOrientationMatrix );

		gl::color( Color::white() );
		gl::translate( mInterfaceCenter );
		gl::scale( Vec3f( mWheelScale, mWheelScale, 1.0f ) );
		mWheelOverlay.draw();
		
		for( int i=0; i<27; i++ ){
			float c = numberAlphaPerChar[i];
			if( c > 0.0f ){
				c += 0.3f;
				gl::color( Color( c, c, c ) );
			} else {
				gl::color( Color( 0.5f, 0, 0 ) );
			}
			mAlphaTextures[i].enableAndBind();
            // TODO: batch these rects (adapt bloom::gl::batchRect for color tints?)
			gl::drawSolidRect( mAlphaRects[i] );
			mAlphaTextures[i].disable();            
		}
		
		glPopMatrix();
	}
}

void AlphaWheel::drawAlphaChar()
{
	//std::cout << "AlphaWheel::drawAlphaChar mAlphaIndex = " << mAlphaIndex << std::endl;
	float w = mAlphaTextures[mAlphaIndex].getWidth() * 0.5f;
	float h = mAlphaTextures[mAlphaIndex].getHeight() * 0.5f;
	
	gl::color( ColorA( BLUE, 1.0f - mWheelScale ) );
	
	gl::enableAdditiveBlending();
    mAlphaTextures[mAlphaIndex].enableAndBind();
	gl::drawSolidRect( Rectf( -w, -h, w, h ) );
	mAlphaTextures[mAlphaIndex].disable();
	gl::enableAlphaBlending();
}

void AlphaWheel::setShowWheel( bool b ){ 
    mShowWheel = b; 
    mCallbacksWheelToggled.call(this);
}

