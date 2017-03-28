/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2017                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/


#include <modules/touch/include/TouchInteraction.h>

#include <openspace/interaction/interactionmode.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/query/query.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/scene/scenegraphnode.h>
#include <openspace/util/time.h>
#include <openspace/util/keys.h>

#include <ghoul/logging/logmanager.h>

#include <glm/gtx/quaternion.hpp>

#ifdef OPENSPACE_MODULE_GLOBEBROWSING_ENABLED
#include <modules/globebrowsing/globes/renderableglobe.h>
#include <modules/globebrowsing/globes/chunkedlodglobe.h>
#include <modules/globebrowsing/geometry/geodetic2.h>
#endif

#include <glm/ext.hpp>

namespace {
    const std::string _loggerCat = "TouchInteraction";
}

using namespace TUIO;
using namespace openspace;

TouchInteraction::TouchInteraction()
	: properties::PropertyOwner("TouchInteraction"),
	_origin("origin", "Origin", ""), 
	_camera{ OsEng.interactionHandler().camera() },
	_baseSensitivity{ 0.1 }, _baseFriction{ 0.02 },
	_vel{ 0.0, glm::dvec2(0.0), glm::dvec2(0.0), 0.0, 0.0 },
	_friction{ _baseFriction, _baseFriction/2.0, _baseFriction, _baseFriction, _baseFriction/4.0 },
	_centroid{ glm::dvec3(0.0) },
	_sensitivity{ 2.0, 0.1, 0.1, 0.1, 0.2 }, 
	_projectionScaleFactor{ 1.000004 },
	_directTouchMode{ false }
{
	_origin.onChange([this]() {
		SceneGraphNode* node = sceneGraphNode(_origin.value());
		if (!node) {
			LWARNING("Could not find a node in scenegraph called '" << _origin.value() << "'");
			return;
		}
		setFocusNode(node);
	});

}

TouchInteraction::~TouchInteraction() { }

void TouchInteraction::update(const std::vector<TuioCursor>& list, std::vector<Point>& lastProcessed) {
	TuioCursor cursor = list.at(0);

	if (_currentRadius > 0.3) // good value to make any planet sufficiently large for direct-touch
		_directTouchMode = true;
	else
		_directTouchMode = false;


	double xCo = 2 * (cursor.getX() - 0.5);
	double yCo = -2 * (cursor.getY() - 0.5); // normalized -1 to 1 coordinates on screen
	glm::dvec3 cursorInWorldSpace = _camera->rotationQuaternion() * glm::dvec3(xCo, yCo, -1.0);
	glm::dvec3 camToCenter = _focusNode->worldPosition() - _camera->positionVec3();
	double dist = std::max(length(glm::cross(cursorInWorldSpace, camToCenter)) / length(cursorInWorldSpace) - _focusNode->boundingSphere().lengthd(), 0.0);

	std::cout << "Dist: " << dist << ", Ray: " << glm::to_string(cursorInWorldSpace) << "\n";
	
	interpret(list, lastProcessed);
	if (!_action.rot) {
		_centroid.x = std::accumulate(list.begin(), list.end(), 0.0f, [](double x, const TuioCursor& c) { return x + c.getX(); }) / list.size();
		_centroid.y = std::accumulate(list.begin(), list.end(), 0.0f, [](double y, const TuioCursor& c) { return y + c.getY(); }) / list.size();
	}
	
	if (_action.rot) { // add rotation velocity
		_vel.globalRot += glm::dvec2(cursor.getXSpeed(), cursor.getYSpeed()) * _sensitivity.globalRot;
	}
	if (_action.pinch) { // add zooming velocity
		double distance = std::accumulate(list.begin(), list.end(), 0.0, [&](double d, const TuioCursor& c) {
			return d + c.getDistance(_centroid.x, _centroid.y);
		});
		double lastDistance = std::accumulate(lastProcessed.begin(), lastProcessed.end(), 0.0f, [&](float d, const Point& p) {
			return d + p.second.getDistance(_centroid.x, _centroid.y);
		});
		
		double zoomFactor = (distance - lastDistance) * glm::distance(_camera->positionVec3(), _camera->focusPositionVec3());
		_vel.zoom += zoomFactor * _sensitivity.zoom;
	}
	if (_action.pan) { // add local rotation velocity
		_vel.localRot += glm::dvec2(cursor.getXSpeed(), cursor.getYSpeed()) * _sensitivity.localRot;
	}
	if (_action.roll) { // add global roll rotation velocity
		double rollFactor = std::accumulate(list.begin(), list.end(), 0.0, [&](double diff, const TuioCursor& c) {
			TuioPoint point = find_if(lastProcessed.begin(), lastProcessed.end(), [&c](const Point& p) { return p.first == c.getSessionID(); })->second;
			double res = diff;
			double lastAngle = point.getAngle(_centroid.x, _centroid.y);
			double currentAngle = c.getAngle(_centroid.x, _centroid.y);
			if (lastAngle > currentAngle + 1.5*M_PI)
				res += currentAngle + (2*M_PI - lastAngle);
			else if (currentAngle > lastAngle + 1.5*M_PI)
				res += (2*M_PI - currentAngle) + lastAngle;
			else
				res += currentAngle - lastAngle;
			return res;
		});
		_vel.localRoll += -rollFactor * _sensitivity.localRoll;
	}
	if (_action.pick) { // pick something in the scene as focus node
		
	}
	//default:
		//LINFO("Couldn't interpret touch input" << "\n");
	//}

}


void TouchInteraction::interpret(const std::vector<TuioCursor>& list, const std::vector<Point>& lastProcessed) {
	double dist = 0;
	double lastDist = 0;
	TuioCursor cursor = list.at(0);
	for (const TuioCursor& c : list) {
		dist += glm::length(glm::dvec2(c.getX(), c.getY()) - glm::dvec2(cursor.getX(), cursor.getY()));
		cursor = c;
	}
	TuioPoint point = lastProcessed.at(0).second;
	for (const Point& p : lastProcessed) {
		dist += glm::length(glm::dvec2(p.second.getX(), p.second.getY()) - glm::dvec2(point.getX(), point.getY()));
		point = p.second;
	}
	if (list.size() == 1) {
		//if(!cursor.isMoving()) // pick

		_action.rot = true;
		_action.pinch = false;
		_action.pan = false;
		_action.roll = false;
		_action.pick = false;
	}
	else {
		if (std::abs(dist - lastDist) / list.size() < 0.1 && list.size() == 2) {
			_action.rot = false;
			_action.pinch = false;
			_action.pan = true;
			_action.roll = false;
			_action.pick = false;
		}
		else {
			_action.rot = false;
			_action.pinch = true;
			_action.pan = false;
			//_action.roll = true;
			_action.pick = false;

			double rollFactor = std::accumulate(list.begin(), list.end(), 0.0, [&](double diff, const TuioCursor& c) {
				TuioPoint point = find_if(lastProcessed.begin(), lastProcessed.end(), [&c](const Point& p) { return p.first == c.getSessionID(); })->second;
				double res = diff;
				double lastAngle = point.getAngle(_centroid.x, _centroid.y);
				double currentAngle = c.getAngle(_centroid.x, _centroid.y);
				if (lastAngle > currentAngle + 1.5*M_PI)
					res += currentAngle + (2 * M_PI - lastAngle);
				else if (currentAngle > lastAngle + 1.5*M_PI)
					res += (2 * M_PI - currentAngle) + lastAngle;
				else
					res += currentAngle - lastAngle;
				return res;
			});
			double maxX = 0.0;
			double minX = 1.0;
			double maxY = 0.0;
			double minY = 1.0;
			for (const TuioCursor& c : list) {
				if (c.getX() > maxX)
					maxX = c.getX();
				if (c.getX() < minX)
					minX = c.getX();
				if (c.getY() > maxY)
					maxY = c.getY();
				if (c.getY() < minY)
					minY = c.getY();
			}
			double xRange = (maxX - minX) / list.size();
			double yRange = (maxY - minY) / list.size();
			if (std::abs(rollFactor) / list.size() > 0.05)
				_action.roll = true;

		}
	}	
}

void TouchInteraction::step(double dt) {
	using namespace glm;

	setFocusNode(OsEng.interactionHandler().focusNode());
	if (_focusNode) {
		// Create variables from current state
		dvec3 camPos = _camera->positionVec3();
		dvec3 centerPos = _focusNode->worldPosition();

		dvec3 directionToCenter = normalize(centerPos - camPos);
		dvec3 centerToCamera = camPos - centerPos;
		dvec3 lookUp = _camera->lookUpVectorWorldSpace();
		dvec3 camDirection = _camera->viewDirectionWorldSpace();

		// Make a representation of the rotation quaternion with local and global rotations
		dmat4 lookAtMat = lookAt(
			dvec3(0, 0, 0),
			directionToCenter,
			normalize(camDirection + lookUp)); // To avoid problem with lookup in up direction
		dquat globalCamRot = normalize(quat_cast(inverse(lookAtMat)));
		dquat localCamRot = inverse(globalCamRot) * _camera->rotationQuaternion();


		double boundingSphere = _focusNode->boundingSphere().lengthd();
		double minHeightAboveBoundingSphere = 1;
		dvec3 centerToBoundingSphere;

		double distance = std::max(length(centerToCamera) - boundingSphere, 0.0);
		_currentRadius = boundingSphere / std::max(distance * _projectionScaleFactor, 1.0);

		{ // Roll
			dquat camRollRot = angleAxis(_vel.localRoll*dt, dvec3(0.0, 0.0, 1.0));
			localCamRot = localCamRot * camRollRot;
		}
		{ // Panning (local rotation)
			dvec3 eulerAngles(_vel.localRot.y*dt, _vel.localRot.x*dt, 0);
			dquat rotationDiff = dquat(eulerAngles);
			localCamRot = localCamRot * rotationDiff;
		}
		{ // Orbit (global rotation)
			dvec3 eulerAngles(_vel.globalRot.y*dt, _vel.globalRot.x*dt, 0);
			dquat rotationDiffCamSpace = dquat(eulerAngles);

			dquat rotationDiffWorldSpace = globalCamRot * rotationDiffCamSpace * inverse(globalCamRot);
			dvec3 rotationDiffVec3 = centerToCamera * rotationDiffWorldSpace - centerToCamera;
			camPos += rotationDiffVec3;

			dvec3 centerToCamera = camPos - centerPos;
			directionToCenter = normalize(-centerToCamera);
			dvec3 lookUpWhenFacingCenter = globalCamRot * dvec3(_camera->lookUpVectorCameraSpace());
			dmat4 lookAtMat = lookAt(
				dvec3(0, 0, 0),
				directionToCenter,
				lookUpWhenFacingCenter);
			globalCamRot = normalize(quat_cast(inverse(lookAtMat)));
		}
		{ // Zooming
			centerToBoundingSphere = -directionToCenter * boundingSphere;
			dvec3 centerToCamera = camPos - centerPos;
			if (length(_vel.zoom*dt) < length(centerToCamera - centerToBoundingSphere) && length(centerToCamera + directionToCenter*_vel.zoom*dt) > length(centerToBoundingSphere)) // should get boundingsphere from focusnode
				camPos += directionToCenter*_vel.zoom*dt;
			else
				_vel.zoom = 0.0;
		}
		{ // Roll around sphere normal
			dquat camRollRot = angleAxis(_vel.globalRoll*dt, -directionToCenter);
			globalCamRot = camRollRot * globalCamRot;
		}
		{ // Push up to surface
			dvec3 sphereSurfaceToCamera = camPos - (centerPos + centerToBoundingSphere);
			double distFromSphereSurfaceToCamera = length(sphereSurfaceToCamera);
			camPos += -directionToCenter * max(minHeightAboveBoundingSphere - distFromSphereSurfaceToCamera, 0.0);
		}
		
		configSensitivities(length(camPos - (centerPos + centerToBoundingSphere)));
		decelerate();

		// Update the camera state
		_camera->setPositionVec3(camPos);
		_camera->setRotation(globalCamRot * localCamRot);
	}
}


void TouchInteraction::configSensitivities(double dist) {
	// Configurates sensitivities to appropriate values when the camera is close to the focus node.
	std::shared_ptr<interaction::GlobeBrowsingInteractionMode> gbim =
		std::dynamic_pointer_cast<interaction::GlobeBrowsingInteractionMode> (OsEng.interactionHandler().interactionmode());
	double close = 4.6 * 1000000;
	if (gbim && dist < close) {
		_sensitivity.zoom = 2.0 * std::max(dist, 100.0)/close;
		_sensitivity.globalRot = 0.1 * std::max(dist, 100.0) /close;
		//_sensitivity.localRot = 0.1;
		//_sensitivity.globalRoll = 0.1;
		//_sensitivity.localRoll = 0.1;
	}
	else {
		_sensitivity.zoom = 2.0;
		_sensitivity.globalRot = 0.1;
		_sensitivity.localRot = 0.1;
		_sensitivity.globalRoll = 0.1;
		_sensitivity.localRoll = 0.2;
	}


}

void TouchInteraction::decelerate() {
	_vel.zoom *= (1 - _friction.zoom);
	_vel.globalRot *= (1 - 0.005);
	_vel.localRot *= (1 - _friction.localRot);
	_vel.globalRoll *= (1 - _friction.globalRoll);
	_vel.localRoll *= (1 - _friction.localRoll);
}

void TouchInteraction::clear() {
	_action.rot = false;
	_action.pinch = false;
	_action.pan = false;
	_action.roll = false;
	_action.pick = false;
}


// Getters
Camera* TouchInteraction::getCamera() {
	return _camera;
}
SceneGraphNode* TouchInteraction::getFocusNode() {
	return _focusNode;
}
double TouchInteraction::getFriction() {
	return _baseFriction;
}
double TouchInteraction::getSensitivity() {
	return _baseSensitivity;
}
// Setters
void TouchInteraction::setCamera(Camera* camera) {
	_camera = camera;
}
void TouchInteraction::setFocusNode(SceneGraphNode* focusNode) {
	_focusNode = focusNode;
}
void TouchInteraction::setFriction(double friction) {
	_baseFriction = std::max(friction, 0.0);
}
void TouchInteraction::setSensitivity(double sensitivity) {
	_baseSensitivity = sensitivity;
}
