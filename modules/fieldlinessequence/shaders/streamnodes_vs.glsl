/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2020                                                               *
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

#version __CONTEXT__
#include "PowerScaling/powerScalingMath.hglsl"
// General Uniforms that's always needed
uniform vec4      lineColor;
uniform mat4      modelViewProjection;

// Uniforms needed to color by quantity
uniform int       colorMode;
uniform sampler1D colorTable;
uniform sampler1D colorTableEarth;
uniform sampler1D colorTableFlow;
uniform vec2      colorTableRange;

// Uniforms needed for Particle Flow
uniform vec4      flowColor;
uniform int       particleSize;
uniform int       particleSpeed;
uniform int       particleSpacing;
uniform bool      usingParticles;
uniform bool      flowColoring;

// Masking Uniforms
uniform bool      usingMasking;
uniform vec2      maskingRange;

// Domain Uniforms
uniform bool      usingDomain;
uniform vec2      domainLimX;
uniform vec2      domainLimY;
uniform vec2      domainLimZ;
uniform vec2      domainLimR;

// Streamnodes specific uniforms
uniform float nodeSize;
uniform float nodeSizeLargerFlux;
uniform vec4 streamColor;
uniform float thresholdFlux;
uniform float filterRadius;
uniform float filterUpper;
uniform int ScalingMode;
uniform int NodeskipMethod;
uniform int Nodeskip;
uniform int Nodeskipdefault;
uniform float NodeskipFluxThreshold;
uniform float NodeskipRadiusThreshold;
uniform float fluxColorAlpha;
uniform vec3 earthPos;
uniform float DistanceThreshold;
uniform int DistanceMethod;
uniform int activestreamnumber;
uniform bool firstrender;
uniform int EnhanceMethod;
uniform double time;

//uniform float interestingStreams[4];

//speicific uniforms for cameraperspective: 
uniform float scaleFactor;
uniform float maxPointSize;
uniform float minPointSize;

uniform mat4 cameraViewProjectionMatrix;
uniform dmat4 modelMatrix;

uniform float correctionSizeFactor;
uniform float correctionSizeEndDistance;
uniform vec3 camerapos;
uniform vec3 up;
uniform vec3 right;
uniform vec3 cameraLookUp;   // in world space (no SGCT View was considered)
uniform vec2 screenSize;

// Inputs
// Should be provided in meters
layout(location = 0) in vec3 in_position;

// The extra value used to color lines. Location must correspond to _VA_COLOR in 
// renderablefieldlinessequence.h
layout(location = 1) in float fluxValue;

// The extra value used to mask out parts of lines. Location must correspond to 
// _VA_MASKING in renderablefieldlinessequence.h
layout(location = 2)
in float rValue;

// The vertex index of every node. Location must correspond to 
// _VA_INDEX in renderableStreamNodes.h
//Using built in gl_vertexID in stead. 
//layout(location = 3)
//in int nodeIndex;
// The vertex streamnumber of every node. Location must correspond to 
// VaStreamnumber in renderableStreamNodes.h
layout(location = 3)
in int Streamnumber;
layout(location = 4) 
in vec2 in_st;

//layout(location = 5) 
//in vec2 arrow;

// These should correspond to the enum 'ColorMode' in renderablestreamnodes.cpp
const int uniformColor     = 0;
const int colorByFluxValue  = 1;

const int uniformskip = 0;
const int Fluxskip = 1;
const int Radiusskip = 2;
const int Streamnumberskip = 3;


const int Fluxmode = 0;
const int RFlux = 1;
const int R2Flux = 2;
const int log10RFlux = 3;
const int lnRFlux = 4;
out vec4 vs_color;
out float vs_depth;
out vec2 vs_st;
//out vec4 vs_gPosition;

vec4 getTransferFunctionColor(sampler1D InColorTable) {
    // Remap the color scalar to a [0,1] range
    float scalevalue = 0;
    if(ScalingMode == Fluxmode){
        scalevalue = fluxValue;
    }
    else if(ScalingMode == RFlux){
       scalevalue = rValue * fluxValue;
    }
    else if(ScalingMode == log10RFlux){
        //conversion from logbase e to log10 since glsl does not support log10. 
        float logtoTen = log(rValue) / log(10);
        scalevalue = logtoTen * fluxValue;
    }
    else if(ScalingMode == lnRFlux){
        scalevalue = log(rValue) * fluxValue;
    }
    else if(ScalingMode == R2Flux){
        scalevalue = rValue * rValue * fluxValue;
    }

    float lookUpVal = (scalevalue - colorTableRange.x)
        /(colorTableRange.y - colorTableRange.x);
    return texture(InColorTable, lookUpVal);
}

bool CheckvertexIndex(){
    
    int nodeIndex = gl_VertexID;
   // nodeIndex = gl_VertexIndex;
    //if(EnhanceMethod == 3) return false;
    if(NodeskipMethod == uniformskip){
        if(mod(nodeIndex, Nodeskip) == 0){
            return true;
        }
    }
    else if(NodeskipMethod == Fluxskip){
        if(fluxValue > NodeskipFluxThreshold && mod(nodeIndex, Nodeskip) == 0){
            return true;
        }
        if(fluxValue < NodeskipFluxThreshold && mod(nodeIndex, Nodeskipdefault) == 0){
            return true;
        }
    }
    else if(NodeskipMethod == Radiusskip){
        if(rValue < NodeskipRadiusThreshold && mod(nodeIndex, Nodeskip) == 0){
            return true;
        }
        if(rValue > NodeskipRadiusThreshold && mod(nodeIndex, Nodeskipdefault) == 0){
            return true;
        }
    }
    else if(NodeskipMethod == Streamnumberskip){
        
    if(Streamnumber == activestreamnumber){
        //vs_color = vec4(0);
        return true;
        }
    }
    return false;
}
//todo fix gl_VertexID

bool isParticle(){

    int modulusResult = int(double(particleSpeed) * time + gl_VertexID) % particleSpacing;
    float speedIrregular = 1;
    if(rValue > 1){
        //if(Streamnumber % 2 == 1)
        //{
            speedIrregular = 4;
            modulusResult = int(double(particleSpeed)* speedIrregular * time + gl_VertexID) % particleSpacing;
        //}
        //else{
        //    modulusResult = int(double(particleSpeed) * time + gl_VertexID) % particleSpacing;
        //}

    }
    else{
            modulusResult = int(double(particleSpeed) * time + gl_VertexID*2) % particleSpacing;
    }
    return modulusResult > 0 && modulusResult <= particleSize;
return false;
}

//function for showing nodes different depending on distance to earth
void DecidehowtoshowClosetoEarth(){
        //Sizescaling
     if(EnhanceMethod == 0){
            float tempR = rValue + 0.4; 
            if(tempR > 1.5){
                tempR = 1.5;
            }
            gl_PointSize = tempR * tempR * tempR * gl_PointSize * 5;
            return;
        }
        //Colortables
      if(EnhanceMethod == 1){
             vec4 fluxColor = getTransferFunctionColor(colorTable);
             vs_color = vec4(fluxColor.xyz, fluxColor.a);
             return;
        }
        //Outline
      if(EnhanceMethod == 2){
            if(!firstrender && vs_color.x != 0 && vs_color.y != 0){
                 gl_PointSize = gl_PointSize + 1;
                 vs_color = vec4(streamColor.xyz, fluxColorAlpha);
            }
            return;
        }
        //lines
      if(EnhanceMethod == 3){
      // Draw every other line grey
      vs_color = vec4(0.18, 0.18, 0.18, 1*fluxColorAlpha);

      float interestingStreams[6] = float[](154, 156, 153, 157, 158, 163);
       // vs_color = vec4(0);
      //float interestingStreams[26] =  float[](135, 138, 145, 146, 147, 149, 153, 154, 155, 156, 157, 158, 159, 160, 167, 163, 
      //168, 169, 170, 172, 174, 180, 181, 183, 356, 364);
      //float interestingStreams[3] = float[](37, 154, 210);
      
      int modulusResult = int(double(particleSpeed) * time + gl_VertexID) % particleSpacing;

      for(int i = 0; i < interestingStreams.length(); i++){
            if(Streamnumber == interestingStreams[i]){
           // if(!firstrender){
               // vs_color = vec4(streamColor.xyz, fluxColorAlpha);
               if(usingParticles && isParticle() && rValue > 0.f){
                   if(modulusResult >= particleSize - 30){

                        if(flowColoring){
                            vec4 fluxColor3 = getTransferFunctionColor(colorTable);
                            vs_color = vec4(fluxColor3.xyz, flowColor.a * 0.8);
                         //vs_color = vec4(1,1,1,1);
                        }
                        else{
                            vs_color = vec4(0.9,0.9,0.9,0.5);
                            //vs_color = flowColor;
                        }
                   }
                   else{
                        vec4 fluxColorFlow = getTransferFunctionColor(colorTableFlow);
                        vs_color = vec4(fluxColorFlow.xyz, 1);
                        }
                        //vs_color = vec4(0.37, 0.37, 0.37, flowColor.a);
            //   }
              // else{
                //   vec4 fluxColor3 = getTransferFunctionColor(colorTable);
                 //  vs_color = vec4(fluxColor3.xyz, fluxColor3.a);
                 // vs_color = vec4(0.37, 0.37, 0.37, flowColor.a);
                }
            }
       }
        //    }
    }
        //SizeandColor
    if(EnhanceMethod == 4){
             vec4 fluxColor3 = getTransferFunctionColor(colorTable);
             vs_color = vec4(fluxColor3.xyz, fluxColor3.a);

            float tempR2 = rValue + 0.4; 
            if(tempR2 > 1.5){
                tempR2 = 1.5;
            }
            gl_PointSize = tempR2 * tempR2 * tempR2 * gl_PointSize * 5;
    }
}

void CheckdistanceMethod() { 
        //Enhance by distance to Earth
        if(EnhanceMethod == 1 || EnhanceMethod == 4){
             vec4 fluxColor2 = getTransferFunctionColor(colorTableEarth);
             vs_color = vec4(fluxColor2.xyz, fluxColor2.a);
        }
        if(DistanceMethod == 0){
             if(distance(earthPos, in_position) < DistanceThreshold){
                DecidehowtoshowClosetoEarth();
             }
        }
        else if(DistanceMethod == 1){
            if(distance(earthPos.x, in_position.x) < DistanceThreshold){
                DecidehowtoshowClosetoEarth();
            }
        }
        else if(DistanceMethod == 2){
            if(distance(earthPos.y, in_position.y) < DistanceThreshold){
                DecidehowtoshowClosetoEarth();
            }
        }
        else if(DistanceMethod == 3){
            if(distance(earthPos.z, in_position.z) < DistanceThreshold){
                DecidehowtoshowClosetoEarth();
            }
        }
}

void main() {
    //vs_color = streamColor;
    // Default gl_PointSize if it is not set anywhere else.
    gl_PointSize = 2;
    // Checking if we should render the vertex dependent on the vertexindex, 
    // by using modulus.
    
    if(CheckvertexIndex()){
    //Filtering by radius and z-axis
    if(rValue > filterRadius && rValue < filterUpper){ //if(rValue > filterRadius){
        if(in_position.z > domainLimZ.x && in_position.z < domainLimZ.y){
            //Uniform coloring
            if(colorMode == 0){
                vs_color = streamColor;
            }
            // We should color it by flux. 
            else{
                vec4 fluxColor = getTransferFunctionColor(colorTable);

                if(fluxValue > thresholdFlux){
                    vs_color = vec4(fluxColor.xyz, fluxColor.a);  
                    gl_PointSize = nodeSizeLargerFlux;
                }
                else{
                    vs_color = vec4(fluxColor.xyz, fluxColorAlpha);
                    gl_PointSize = nodeSize;
                }
            }
             CheckdistanceMethod();
        }
        else{
            vs_color = vec4(0);
            }
        }
    else{
        vs_color = vec4(0);
        }
    }
    else{
        vs_color = vec4(0);
    }
   
  
    /*
    if(distance(in_position, camerapos) < 100000000000.f){
        gl_PointSize = nodeSize * 5;
     }
    else{
    gl_PointSize = nodeSize;
    }
    */
    //test for camera perspective:: 
    dvec4 dpos = dvec4(in_position, 1.0);
    dpos = modelMatrix * dpos;

    float scaleMultiply = exp(scaleFactor * 0.10f);

    vec3 scaledRight    = vec3(0.f);
    vec3 scaledUp       = vec3(0.f);

    vec3 normal   = vec3(normalize(camerapos - dpos.xyz));
        vec3 newRight = normalize(cross(cameraLookUp, normal));
        vec3 newUp    = cross(normal, newRight);

     double distCamera = length(camerapos - dpos.xyz);
     float expVar = float(-distCamera) / pow(10.f, correctionSizeEndDistance);
     float factorVar = pow(10.f, correctionSizeFactor);
     scaleMultiply *= 1.f / (1.f + factorVar * exp(expVar));

    //vec2 halfViewSize = vec2(screenSize.x, screenSize.y) * 0.5f;
      //  vec2 topRight = crossCorner.xy/crossCorner.w;
       // vec2 bottomLeft = initialPosition.xy/initialPosition.w;
        
        // width and height
        //vec2 sizes = abs(halfViewSize * (topRight - bottomLeft));
        //float ta = 1.0f;
    /*
    if (sizes.x < 2.0f * minPointSize) {
                float maxVar = 2.0f * minPointSize;
                float minVar = minPointSize;
                float var    = (sizes.y + sizes.x);
                ta = ( (var - minVar)/(maxVar - minVar) );
               
                if (ta == 0.0f)
                    return;
            }
             gl_PointSize = ta;
        }
        */
       
    scaledRight = scaleMultiply * right * 0.5f;
    scaledUp    = scaleMultiply * up * 0.5f;

    vec4 dposClip = cameraViewProjectionMatrix * vec4(dpos);
    vec4 scaledRightClip = cameraViewProjectionMatrix * vec4(scaledRight, 0.0);
    vec4 scaledUpClip = cameraViewProjectionMatrix * vec4(scaledUp, 0.0);

    vec4 initialPosition = z_normalization(dposClip - scaledRightClip - scaledUpClip);
    vs_depth = initialPosition.w;
    gl_Position = initialPosition;
    float maxdist = 600000000000.f;
    float maxdist2 = 60000000000.f;
    float distancevec = distance(camerapos, in_position.xyz);
    
     if(distancevec < maxdist){
     float distScale = 1 - smoothstep(0, maxdist, distancevec);
    /*
    if(distScale <= 0.5f){
     distScale += 0.5f;
     }
     else{
     distScale += -0.5f;
     }
     */
     float factorS = pow(distScale, 9) * 80.f;
     if(gl_PointSize * factorS > 30){
        gl_PointSize = 30;
     }
     else{
     gl_PointSize = gl_PointSize * factorS;
     }
     }
     else{
     gl_PointSize = 2.f;
     }
    //if(!firstrender){
    //CheckdistanceMethod();
   // }
    
    //temporary things for trying out point sprites. 
      /*  if(!firstrender && vs_color.w != 0){
            vs_st = in_st;
        }
        else{
        vs_st = vec2(-1);
        }
        */

        vec4 position_in_meters = vec4(in_position, 1);
        vec4 positionClipSpace = modelViewProjection * position_in_meters;
        //vs_gPosition = vec4(modelViewTransform * dvec4(in_point_position, 1));
      
       // gl_PointSize = nodeSize;
        //gl_Position = vec4(positionClipSpace.xy, 0, positionClipSpace.w);
       // vs_depth = gl_Position.w;
        
       // if(distance(positionClipSpace.xyz, camerapos) < 0.f){
       
        
        
}