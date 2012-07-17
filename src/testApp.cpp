/**
 * Example - Scene With Timeline
 * This example shows how to create a basic RGB+D scene
 * with a timeline to scrub through the video.
 *
 * It requiers ofxGameCamera https://github.com/Flightphase/ofxGameCamera 
 * and ofxTimeline https://github.com/Flightphase/ofxTimeline in addition to ofxRGBDepth
 *
 * James George 2012 
 * Released under the MIT License
 *
 * The RGBDToolkit has been developed with support from the STUDIO for Creative Inquiry and Eyebeam
 */

#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
    
    ofSetFrameRate(60);
    ofSetVerticalSync(true);
    ofBackground(25);
    
    //set up the game camera
    cam.setup();
    cam.speed = 20;
    cam.autosavePosition = true;
    cam.targetNode.setPosition(ofVec3f());
    cam.targetNode.setOrientation(ofQuaternion());
    cam.targetXRot = -180;
    cam.targetYRot = 0;
    cam.rotationZ = 0;    
    cam.setScale(1,-1,1);

    xshift = 0;
    yshift = 0;
    maxFeatures = 500;
    featureQuality = .5;
	minDistance = 10;
	
    
    gui.setup("tests");
    gui.add(loadNew.setup("load new scene"));

    gui.add(xshift.setup("xshift", ofxParameter<float>(), -.15, .15));
    gui.add(yshift.setup("yshift", ofxParameter<float>(), -.15, .15));
    gui.add(farClip.setup("far clip", ofxParameter<float>(),1000, 4500));
    
    gui.add(maxFeatures.setup("max features", ofxParameter<int>(),100, 5000));
    gui.add(featureQuality.setup("feature quality", ofxParameter<float>(),.0000001, .01));
    gui.add(minDistance.setup("min distance", ofxParameter<float>(), .0, 200));
	gui.add(renderMode.setup("render face tracking", ofxParameter<bool>()));
    gui.add(clownNose.setup("clown nose", ofxParameter<bool>()));

    gui.loadFromFile("defaultSettings.xml");
    renderMode = false;
    
    calculateRects();
    
    //attemping to load the last scene
    loadDefaultScene();
    
    faceTracker.setup();
    faceTrackerMesh = ofMesh();
    
    
    ofEnableSmoothing(); 
    ofSetSmoothLighting(true);
    light = new ofLight();
    lightColor = *new ofColor(215, 225, 215);
    lightColor.setBrightness( 255.f );
    lightColor.setSaturation( 130.f );
    light->setPosition(500, 500, 500);
    light->setDiffuseColor( lightColor );
    
}

//--------------------------------------------------------------
bool testApp::loadNewScene(){
    ofFileDialogResult r = ofSystemLoadDialog("Select a Scene", true);
    if(r.bSuccess){
        return loadScene(r.getPath());
    }
    return false;
}

//--------------------------------------------------------------
bool testApp::loadDefaultScene(){
    ofxXmlSettings settings;
    if(settings.loadFile("RGBDSimpleSceneDefaults.xml")){
        if(!loadScene(settings.getValue("defaultScene", ""))){
            return loadNewScene();
        }
        return true;
    }
    return loadNewScene();
}

//--------------------------------------------------------------
bool testApp::loadScene(string takeDirectory){
    if(player.setup(takeDirectory, true)){
        ofxXmlSettings settings;
        settings.loadFile("RGBDSimpleSceneDefaults.xml");
        settings.setValue("defaultScene", player.getScene().mediaFolder);
        settings.saveFile();
        
        meshBuilder.setup(player.getScene().calibrationFolder);
        
        //populate
        player.getVideoPlayer().setPosition(.5);
        player.update();
        
        meshBuilder.setXYShift(player.getXYShift());
        //this will compensate if we are using an offline video that is of a different scale
        //meshBuilder.setTextureScaleForImage(player.getVideoPlayer()); 
        //update the first mesh
        meshBuilder.updateMesh(player.getDepthPixels());

        return true;
    }
    return false;
}

//--------------------------------------------------------------
void testApp::update(){
    if(loadNew){
        loadNewScene();
    }
    
    cam.applyRotation = cam.applyTranslation = meshRect.inside(mouseX, mouseY) || triangulatedRect.inside(mouseX, mouseY);
    
    //copy any GUI changes into the mesh builder
    if(meshBuilder.shift.x != xshift || meshBuilder.shift.y != yshift || meshBuilder.farClip != farClip){
        meshBuilder.setXYShift( ofVec2f(xshift,yshift) );
		meshBuilder.farClip = farClip;
        meshBuilder.updateMesh(player.getDepthPixels());
    }
    
    //update the mesh if there is a new depth frame in the player
    player.update();
    if(player.isFrameNew()){
        meshBuilder.updateMesh(player.getDepthPixels());
        faceTracker.update(toCv(player.getVideoPlayer()));
    }
    
    if(renderMode){
//        if(!player.getVideoPlayer().isPlaying()){
//            player.getVideoPlayer().play();
//            player.getVideoPlayer().setSpeed(0);
//        }

        player.getVideoPlayer().setFrame( player.getVideoPlayer().getCurrentFrame() + 1);
        player.update();
        
        
        createFaceMesh(.5);
	    //createTriangulation();

    }
}

//--------------------------------------------------------------
void testApp::draw(){
    if(player.isLoaded()){
        
        //draw video player...
        player.getVideoPlayer().draw(videoRect);
        
        //...with feature overlay
        ofPushMatrix();
        ofTranslate(videoRect.x, videoRect.y);
        ofScale(videoRect.width / player.getVideoPlayer().getWidth(), 
                videoRect.height / player.getVideoPlayer().getHeight());
        
        ofPushStyle();
        ofNoFill();
        ofSetColor(255, 0, 0);
        //triangulate.triangleMesh.drawWireframe();
        faceTracker.draw(true);
        ofPopStyle();
        ofPopMatrix();
        
        //draw standard generated mesh
        previewFBO.begin();

        ofClear(0,0,0,0);
        
        cam.begin();
              
        glEnable(GL_DEPTH_TEST);
        
        meshBuilder.draw(player.getVideoPlayer());
        
        //I isolated the lighting in here so it doesn't mess with the meshBuilder mesh
        if(faceTrackerMesh.hasVertices() && clownNose){
            
            ofPushStyle();
            ofEnableLighting();  
            light->enable();
            ofSetColor(255, 0, 0);
            ofScale(1,-1, 1);
            ofSphere(faceTrackerMesh.getVertex(30).x, faceTrackerMesh.getVertex(30).y, faceTrackerMesh.getVertex(30).z - 15, 20);
            light->disable();
            ofDisableLighting();
            ofPopStyle();
        }

        glDisable(GL_DEPTH_TEST);
        cam.end();
        previewFBO.end();
        previewFBO.getTextureReference().draw(meshRect);
        
        
        //draw triangulated mesh
        renderFBO.begin();
        ofClear(0,0,0,0);
        cam.begin();
        ofPushMatrix();
        ofPushStyle();
        ofScale(1,-1, 1);
        glEnable(GL_DEPTH_TEST);
        player.getVideoPlayer().getTextureReference().bind();
        ofEnableAlphaBlending();
        ofSetColor(255, 255, 255, 100);
        ofBlendMode(OF_BLENDMODE_ADD);
        
        ofPushStyle();
        ofSetLineWidth(2);
        glEnable(GL_FLAT);
        ofSetColor(255, 255, 255);
        faceTrackerMesh.drawWireframe();
        ofPopStyle();
        
        player.getVideoPlayer().getTextureReference().unbind();
        glDisable(GL_DEPTH_TEST);
        ofPopStyle();
        ofPopMatrix();
        cam.end();
        renderFBO.end();
        
        renderFBO.getTextureReference().draw(triangulatedRect);
        if(renderMode){
            ofImage saveImage;
            renderFBO.getTextureReference().readToPixels(saveImage.getPixelsRef());
            char filename[1024];
            sprintf(filename, "videframe_%05d.png", player.getVideoPlayer().getCurrentFrame());
            saveImage.saveImage(filename);
        }
    }
    
    //faceTrackerMesh.drawWireframe();
    
    gui.draw();
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
    if(key == ' '){
        player.togglePlay();
    }
}

//--------------------------------------------------------------
//The most important function of the FaceTrackerMesh demo. 
void testApp::createFaceMesh(float minDist){
    
    faceTrackerMesh.clear();
    
    //Size of faceTracker image mesh and a container for the 2D mesh
    int faceMeshSize = faceTracker.getImageMesh().getNumVertices();
    ofMesh tempMesh = faceTracker.getImageMesh();
    
    //Texture coords from main mesh
    vector<ofVec2f>& textureCoords = meshBuilder.getMesh().getTexCoords();
    vector<bool> validVertIndeces;

    //Do a linear search to find the closest texture coordinate
    for(int i = 0; i < faceMeshSize; i++){
        int closestTexCoordIndex = 0;
        float closestTexCoordDistance = FLT_MAX;
    
        for(int j = 0; j < textureCoords.size(); j++){
            
            ofVec2f v2d(tempMesh.getVertex(i).x, tempMesh.getVertex(i).y);
            float texCoordDist = v2d.distanceSquared(textureCoords[j]);
            
            if(texCoordDist < closestTexCoordDistance){
                closestTexCoordDistance = texCoordDist;
                closestTexCoordIndex = j;
            }
            
            //Break the loop if the distance is closer than minDist to save
            //time.
            if(texCoordDist < minDist) break;
            
        }
        
        
        ofVec3f vert = meshBuilder.getMesh().getVertex(closestTexCoordIndex);
        
        faceTrackerMesh.addVertex(vert);
        faceTrackerMesh.addTexCoord(meshBuilder.getMesh().getTexCoord(closestTexCoordIndex));
        validVertIndeces.push_back(vert.z < farClip && vert.z > 10);
    }
    
    //Okay, now we want to make the indices/triangles from the F.T. imageMesh
    //and the faceTrackerMesh correspond
    faceTrackerMesh.addIndices(faceTracker.getImageMesh().getIndices());
    
    //Calculate and add normals, copied somewhat from the Delauney Tracker. Shouldn't
    //be too important, but regardless I would prefer having a mesh with as much info
    //possible.
    for(int i = 0; i < faceTrackerMesh.getNumIndices(); i += 3){
        
        ofIndexType a, b, c;
        
        a = faceTrackerMesh.getIndex(i);
        if(!validVertIndeces[a]) { continue; }
        
        b = faceTrackerMesh.getIndex(i + 1);
        if(!validVertIndeces[b]) { continue; }
        
        c = faceTrackerMesh.getIndex(i + 2);
        if(!validVertIndeces[c]) { continue; }
        
        ofVec3f posA = faceTrackerMesh.getVerticesPointer()[i];
        ofVec3f posB = faceTrackerMesh.getVerticesPointer()[(i+1)];
        ofVec3f posC = faceTrackerMesh.getVerticesPointer()[(i+2)];
        
        ofVec3f norm = (posA - posC).getCrossed(posB - posC).getNormalized();
        faceTrackerMesh.addNormal(norm);
        faceTrackerMesh.addNormal(norm);
        faceTrackerMesh.addNormal(norm);
    }
}


//--------------------------------------------------------------
void testApp::calculateRects(){
    float rectWidth = ofGetWidth()/2;
    float rectHeight = ofGetWidth()/2 * (9./16.);
    videoRect = ofRectangle(rectWidth,0,rectWidth,rectHeight);
    meshRect = ofRectangle(0,rectHeight,rectWidth,rectHeight);
	triangulatedRect = ofRectangle(rectWidth,rectHeight,rectWidth,rectHeight);
    renderFBO.allocate(rectWidth, rectHeight, GL_RGB, 4);
    previewFBO.allocate(rectWidth, rectHeight, GL_RGB, 4);
}

//--------------------------------------------------------------
void testApp::exit(){
    gui.saveToFile("defaultSettings.xml");
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){ 

}