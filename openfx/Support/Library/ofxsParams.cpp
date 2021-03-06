/*
OFX Support Library, a library that skins the OFX plug-in API with C++ classes.
Copyright (C) 2004-2005 The Open Effects Association Ltd
Author Bruno Nicoletti bruno@thefoundry.co.uk

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
* Neither the name The Open Effects Association Ltd, nor the names of its 
contributors may be used to endorse or promote products derived from this
software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The Open Effects Association Ltd
1 Wardour St
London W1D 6PA
England


*/

/** @brief This file contains code that skins the ofx param suite */

#include <cstring>
#ifdef DEBUG
#include <iostream>
#endif
#include "ofxsSupportPrivate.h"
#include "ofxParametricParam.h"
#ifdef OFX_EXTENSIONS_NUKE
#include "nuke/camera.h"
#endif

#ifdef DEBUG
// We check that strings contained in StringParam are UTF-8 encoded
// See http://openfx.sourceforge.net/Documentation/1.4/ofxProgrammingReference.html#kOfxParamTypeString
//
// From http://stackoverflow.com/questions/1031645/how-to-detect-utf-8-in-plain-c
static bool is_utf8(const char * string)
{
  if(!string)
    return 0;
  
  const unsigned char * bytes = (const unsigned char *)string;
  while(*bytes)
  {
    if( (// ASCII
         // use bytes[0] <= 0x7F to allow ASCII control characters
         bytes[0] == 0x09 ||
         bytes[0] == 0x0A ||
         bytes[0] == 0x0D ||
         (0x20 <= bytes[0] && bytes[0] <= 0x7E)
         )
       ) {
      bytes += 1;
      continue;
    }
    
    if( (// non-overlong 2-byte
         (0xC2 <= bytes[0] && bytes[0] <= 0xDF) &&
         (0x80 <= bytes[1] && bytes[1] <= 0xBF)
         )
       ) {
      bytes += 2;
      continue;
    }
    
    if( (// excluding overlongs
         bytes[0] == 0xE0 &&
         (0xA0 <= bytes[1] && bytes[1] <= 0xBF) &&
         (0x80 <= bytes[2] && bytes[2] <= 0xBF)
         ) ||
       (// straight 3-byte
        ((0xE1 <= bytes[0] && bytes[0] <= 0xEC) ||
         bytes[0] == 0xEE ||
         bytes[0] == 0xEF) &&
        (0x80 <= bytes[1] && bytes[1] <= 0xBF) &&
        (0x80 <= bytes[2] && bytes[2] <= 0xBF)
        ) ||
       (// excluding surrogates
        bytes[0] == 0xED &&
        (0x80 <= bytes[1] && bytes[1] <= 0x9F) &&
        (0x80 <= bytes[2] && bytes[2] <= 0xBF)
        )
       ) {
      bytes += 3;
      continue;
    }
    
    if( (// planes 1-3
         bytes[0] == 0xF0 &&
         (0x90 <= bytes[1] && bytes[1] <= 0xBF) &&
         (0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
         (0x80 <= bytes[3] && bytes[3] <= 0xBF)
         ) ||
       (// planes 4-15
        (0xF1 <= bytes[0] && bytes[0] <= 0xF3) &&
        (0x80 <= bytes[1] && bytes[1] <= 0xBF) &&
        (0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
        (0x80 <= bytes[3] && bytes[3] <= 0xBF)
        ) ||
       (// plane 16
        bytes[0] == 0xF4 &&
        (0x80 <= bytes[1] && bytes[1] <= 0x8F) &&
        (0x80 <= bytes[2] && bytes[2] <= 0xBF) &&
        (0x80 <= bytes[3] && bytes[3] <= 0xBF)
        )
       ) {
      bytes += 4;
      continue;
    }
    
    return 0;
  }
  
  return 1;
}
#endif // DEBUG

/** @brief The core 'OFX Support' namespace, used by plugin implementations. All code for these are defined in the common support libraries. */
namespace OFX {

  /** @brief dummy page positioning parameter to be passed to @ref OFX::PageParamDescriptor::addChild */
  DummyParamDescriptor PageParamDescriptor::gSkipRow(kOfxParamPageSkipRow);

  /** @brief dummy page positioning parameter to be passed to @ref OFX::PageParamDescriptor::addChild */
  DummyParamDescriptor PageParamDescriptor::gSkipColumn(kOfxParamPageSkipColumn);

  /** @brief turns a ParamTypeEnum into the char * that raw OFX uses */
  const char * mapParamTypeEnumToString(ParamTypeEnum v)
  {
    switch(v) 
    {
    case eStringParam : return kOfxParamTypeString ;
    case eIntParam : return kOfxParamTypeInteger ;
    case eInt2DParam : return kOfxParamTypeInteger2D ;
    case eInt3DParam : return kOfxParamTypeInteger3D ;
    case eDoubleParam : return kOfxParamTypeDouble ;
    case eDouble2DParam : return kOfxParamTypeDouble2D ;
    case eDouble3DParam : return kOfxParamTypeDouble3D ;
    case eRGBParam : return kOfxParamTypeRGB ;
    case eRGBAParam : return kOfxParamTypeRGBA ;
    case eBooleanParam : return kOfxParamTypeBoolean ;
    case eChoiceParam : return kOfxParamTypeChoice ;
    case eCustomParam : return kOfxParamTypeCustom ;
    case eGroupParam : return kOfxParamTypeGroup ;
    case ePageParam : return kOfxParamTypePage ;
    case ePushButtonParam : return kOfxParamTypePushButton ;
    case eParametricParam : return kOfxParamTypeParametric ;
    default: assert(false);
    }
    return kOfxParamTypeInteger;
  }

  static
  bool isEqual(const char* t1, const char* t2)
  {
    return strcmp(t1, t2)==0;
  }

  static
  ParamTypeEnum mapParamTypeStringToEnum(const char * v)
  {
    if(isEqual(kOfxParamTypeString,v)) 
      return eStringParam ;
    else if(isEqual(kOfxParamTypeInteger,v)) 
      return eIntParam ;
    else if(isEqual(kOfxParamTypeInteger2D,v)) 
      return eInt2DParam ;
    else if(isEqual(kOfxParamTypeInteger3D,v)) 
      return eInt3DParam ;
    else if(isEqual(kOfxParamTypeDouble,v)) 
      return eDoubleParam ;
    else if(isEqual(kOfxParamTypeDouble2D,v))
      return eDouble2DParam ;
    else if(isEqual(kOfxParamTypeDouble3D,v)) 
      return eDouble3DParam ;
    else if(isEqual(kOfxParamTypeRGB,v))
      return eRGBParam ;
    else if(isEqual(kOfxParamTypeRGBA,v))
      return eRGBAParam ;
    else if(isEqual(kOfxParamTypeBoolean,v))
      return eBooleanParam ;
    else if(isEqual(kOfxParamTypeChoice,v))
      return eChoiceParam ;
    else if(isEqual(kOfxParamTypeCustom ,v))
      return eCustomParam ;
    else if(isEqual(kOfxParamTypeGroup,v))
      return eGroupParam ;
    else if(isEqual(kOfxParamTypePage,v)) 
      return ePageParam ;
    else if(isEqual(kOfxParamTypePushButton,v))
      return ePushButtonParam ;
    else if(isEqual(kOfxParamTypeParametric,v))
      return eParametricParam ;
    else
      assert(false);
    return ePushButtonParam ;
  }

#ifdef OFX_EXTENSIONS_VEGAS
  /** @brief map a std::string to a keyframe interpolation type */
  VegasInterpolationEnum mapToInterpolationEnum(const std::string &s) throw(std::invalid_argument)
  {
    if(s == kOfxVegasKeyframeInterpolationUnknown) return eVegasInterpolationUnknown;
    if(s == kOfxVegasKeyframeInterpolationLinear)  return eVegasInterpolationLinear;
    if(s == kOfxVegasKeyframeInterpolationFast)    return eVegasInterpolationFast; 
    if(s == kOfxVegasKeyframeInterpolationSlow)    return eVegasInterpolationSlow; 
    if(s == kOfxVegasKeyframeInterpolationSmooth)  return eVegasInterpolationSmooth; 
    if(s == kOfxVegasKeyframeInterpolationSharp)   return eVegasInterpolationSharp;  
    if(s == kOfxVegasKeyframeInterpolationHold)    return eVegasInterpolationHold;  
    if(s == kOfxVegasKeyframeInterpolationManual)  return eVegasInterpolationManual; 
    if(s == kOfxVegasKeyframeInterpolationSplit)   return eVegasInterpolationSplit;
    OFX::Log::error(true, "Unknown keyframe Interpolation '%s'", s.c_str());
    throw std::invalid_argument(s);
  }

  const char* mapToInterpolationTypeEnum(OFX::VegasInterpolationEnum type)
  {
         if(type == OFX::eVegasInterpolationUnknown) return kOfxVegasKeyframeInterpolationUnknown;
    else if(type == OFX::eVegasInterpolationLinear)  return kOfxVegasKeyframeInterpolationLinear; 
    else if(type == OFX::eVegasInterpolationFast)    return kOfxVegasKeyframeInterpolationFast;   
    else if(type == OFX::eVegasInterpolationSlow)    return kOfxVegasKeyframeInterpolationSlow;   
    else if(type == OFX::eVegasInterpolationSmooth)  return kOfxVegasKeyframeInterpolationSmooth; 
    else if(type == OFX::eVegasInterpolationSharp)   return kOfxVegasKeyframeInterpolationSharp;  
    else if(type == OFX::eVegasInterpolationHold)    return kOfxVegasKeyframeInterpolationHold;   
    else if(type == OFX::eVegasInterpolationManual)  return kOfxVegasKeyframeInterpolationManual; 
    else if(type == OFX::eVegasInterpolationSplit)   return kOfxVegasKeyframeInterpolationSplit;  
    OFX::Log::error(true, "Unknown interpolation type enum '%d'", type);
    return 0;
  }
#endif

  ////////////////////////////////////////////////////////////////////////////////
  // the base class for all param descriptors

  /** @brief ctor */
  ParamDescriptor::ParamDescriptor(const std::string &name, ParamTypeEnum type, OfxPropertySetHandle props)
    : _paramName(name)
    , _paramType(type)
    , _paramProps(props)
  {
    // validate the properities on this descriptor
    if(type != eDummyParam)
      OFX::Validation::validateParameterProperties(type, props, true); 
  }

  ParamDescriptor::~ParamDescriptor()
  {
  }

  /** @brief set the label property */
  void 
    ParamDescriptor::setLabel(const std::string &label)
  {
    _paramProps.propSetString(kOfxPropLabel, label);
  }

  /** @brief set the label properties */
  void 
    ParamDescriptor::setLabels(const std::string &label, const std::string &shortLabel, const std::string &longLabel)
  {
    setLabel(label);
    _paramProps.propSetString(kOfxPropShortLabel, shortLabel, false);
    _paramProps.propSetString(kOfxPropLongLabel, longLabel, false);
  }

  /** @brief set the param hint */
  void 
    ParamDescriptor::setHint(const std::string &v)
  {
    _paramProps.propSetString(kOfxParamPropHint, v, false);
  }

  /** @brief set the script name, default is the name it was defined with */
  void
    ParamDescriptor::setScriptName(const std::string &v)
  {
    _paramProps.propSetString(kOfxParamPropScriptName, v, false);
  }

  /** @brief set the secretness of the param, defaults to false */
  void 
    ParamDescriptor::setIsSecret(bool v)
  {
    _paramProps.propSetInt(kOfxParamPropSecret, v);
  }

  /** @brief set the if the param is enabled, defaults to true */
  void 
    ParamDescriptor::setEnabled(bool v)
  {
    _paramProps.propSetInt(kOfxParamPropEnabled, v);
  }

#ifdef OFX_EXTENSIONS_VEGAS
  /** @brief set if the parameter is expanded (Vegas specific), defaults to true */
  void 
    ParamDescriptor::setParameterExpanded(bool v)
  {
    _paramProps.propSetInt(kOfxParamPropParameterExpanded, v, false);
  }
#endif

#ifdef OFX_EXTENSIONS_NUKE
  void
    ParamDescriptor::setLayoutHint(const ELayoutHint layoutHint, int padWidth)
  {
    _paramProps.propSetInt(kOfxParamPropLayoutHint, static_cast<int>(layoutHint), false);
    if (layoutHint == eLayoutHintNoNewLine) {
      _paramProps.propSetInt(kOfxParamPropLayoutPadWidth, padWidth, false);
    }
  }
#endif
    
#ifdef OFX_EXTENSIONS_NATRON
    /** @brief When set to true, the parameter is specific to an effect instance of the plug-in and should have a
     unique representation for each instance. See descripton of kNatronOfxImageEffectContextTracker for more details
     on multiple instances and difference between shared and specific parameters.*/
  void
    ParamDescriptor::setInstanceSpecific(bool isSpecific)
  {
    _paramProps.propSetInt(kNatronOfxParamPropIsInstanceSpecific, (int)isSpecific, false);
  }

  /*Indicates if the plug-in description is written in markdown or plain-text otherwise */
  void
    ParamDescriptor::setDescriptionIsMarkdown(bool markdown)
  {
    _paramProps.propSetInt(kNatronOfxPropDescriptionIsMarkdown, (int)markdown, 0, false);
  }

  void
    ParamDescriptor::setInViewportLayoutHint(const ELayoutHint layoutHint, int padWidth)
  {
    _paramProps.propSetInt(kNatronOfxParamPropInViewerContextLayoutHint, static_cast<int>(layoutHint), false);
    if (layoutHint == eLayoutHintNoNewLine) {
      _paramProps.propSetInt(kNatronOfxParamPropInViewerContextLayoutPadWidth, padWidth, false);
    }
  }

  void
    ParamDescriptor::setInViewportLabel(const std::string& label)
  {
    _paramProps.propSetString(kNatronOfxParamPropInViewerContextLabel, label, 0);
  }

  void
    ParamDescriptor::setInViewportIsSecret(bool secret)
  {
    _paramProps.propSetInt(kNatronOfxParamPropInViewerContextSecret, (int)secret);
  }
#endif

  /** @brief set the group param that is the parent of this one, default is to be ungrouped at the root level */
  void 
    ParamDescriptor::setParent(const GroupParamDescriptor &v)
  {
    _paramProps.propSetString(kOfxParamPropParent, v.getName());
  }

  /** @brief set the icon file name (SVG or PNG) */
  void 
    ParamDescriptor::setIcon(const std::string &v, bool pngFormat)
  {
    _paramProps.propSetString(kOfxPropIcon, v, (int)pngFormat, false); // introduced in OFX 1.2
  }
    
  bool
  ParamDescriptor::getHostHasNativeOverlayHandle() const
  {
    bool v = _paramProps.propGetInt(kOfxParamPropHasHostOverlayHandle, 0, false) != 0; // OFX 1.2
    return v;
  }
    
  void
  ParamDescriptor::setUseHostNativeOverlayHandle(bool use)
  {
    _paramProps.propSetInt(kOfxParamPropUseHostOverlayHandle, use, 0, false); // OFX 1.2
  }

  ////////////////////////////////////////////////////////////////////////////////
  // the base class for all params that can hold a value

  /** @brief ctor */
  ValueParamDescriptor::ValueParamDescriptor(const std::string &name, ParamTypeEnum type, OfxPropertySetHandle props)
    : ParamDescriptor(name, type, props)
  {
  }

  /** @brief dtor */
  ValueParamDescriptor::~ValueParamDescriptor()
  {
  }

  /** @brief set whether the param can animate, defaults to true in most cases */
  void ValueParamDescriptor::setAnimates(bool v)
  {
    _paramProps.propSetInt(kOfxParamPropAnimates, v);
  }

  /** @brief set whether the param is persistent, defaults to true */
  void ValueParamDescriptor::setIsPersistent(bool v)
  {
    _paramProps.propSetInt(kOfxParamPropPersistent, v);
  }

  /** @brief Set's whether the value of the param is significant (ie: affects the rendered image), defaults to true */
  void ValueParamDescriptor::setEvaluateOnChange(bool v)
  {
    _paramProps.propSetInt(kOfxParamPropEvaluateOnChange, v);
  }

  /** @brief Set's whether the value of the param is significant (ie: affects the rendered image), defaults to true */
  void ValueParamDescriptor::setCanUndo(bool v)
  {
    _paramProps.propSetInt(kOfxParamPropCanUndo, v, 0, false);
  }

  /** @brief Set's how any cache should be invalidated if the parameter is changed, defaults to eCacheInvalidateValueChange */
  void ValueParamDescriptor::setCacheInvalidation(CacheInvalidationEnum v)
  {
    switch(v) 
    {
    case eCacheInvalidateValueChange :
      _paramProps.propSetString(kOfxParamPropCacheInvalidation, kOfxParamInvalidateValueChange);
      break;

    case eCacheInvalidateValueChangeToEnd :
      _paramProps.propSetString(kOfxParamPropCacheInvalidation, kOfxParamInvalidateValueChangeToEnd);
      break;

    case eCacheInvalidateValueAll :
      _paramProps.propSetString(kOfxParamPropCacheInvalidation, kOfxParamInvalidateAll);
      break;
    }
  }

  void ValueParamDescriptor::setInteractDescriptor(ParamInteractDescriptor* desc)
  {
    _interact.reset(desc);
    _paramProps.propSetPointer(kOfxParamPropInteractV1, (void*)desc->getMainEntry());
    desc->setParamName(getName());
  }

  ////////////////////////////////////////////////////////////////////////////////
  // int param descriptor

  /** @brief ctor */
  IntParamDescriptor::IntParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : ValueParamDescriptor(name, eIntParam, props)
  {
  }

  /** @brief set the default value, default is 0 */
  void 
    IntParamDescriptor::setDefault(int v)
  {
    _paramProps.propSetInt(kOfxParamPropDefault, v);
  }

  /** @brief set the hard min/max range, default is INT_MIN, INT_MAX */
  void 
    IntParamDescriptor::setRange(int min, int max)
  {
    _paramProps.propSetInt(kOfxParamPropMin, min);
    _paramProps.propSetInt(kOfxParamPropMax, max);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    IntParamDescriptor::setDisplayRange(int min, int max)
  {
    _paramProps.propSetInt(kOfxParamPropDisplayMin, min);
    _paramProps.propSetInt(kOfxParamPropDisplayMax, max);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // 2D int param descriptor

  /** @brief ctor */
  Int2DParamDescriptor::Int2DParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : ValueParamDescriptor(name, eInt2DParam, props)
  {
  }

  /** @brief set the default value, default is 0 */
  void 
    Int2DParamDescriptor::setDefault(int x, int y)
  {
    _paramProps.propSetInt(kOfxParamPropDefault, x, 0);
    _paramProps.propSetInt(kOfxParamPropDefault, y, 1);
  }

  /** @brief set the hard min/max range, default is INT_MIN, INT_MAX */
  void 
    Int2DParamDescriptor::setRange(int xmin, int ymin,
    int xmax, int ymax)
  {
    _paramProps.propSetInt(kOfxParamPropMin, xmin, 0);
    _paramProps.propSetInt(kOfxParamPropMin, ymin, 1);
    _paramProps.propSetInt(kOfxParamPropMax, xmax, 0);
    _paramProps.propSetInt(kOfxParamPropMax, ymax, 1);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    Int2DParamDescriptor::setDisplayRange(int xmin, int ymin,
    int xmax, int ymax)
  {
    _paramProps.propSetInt(kOfxParamPropDisplayMin, xmin, 0);
    _paramProps.propSetInt(kOfxParamPropDisplayMin, ymin, 1);
    _paramProps.propSetInt(kOfxParamPropDisplayMax, xmax, 0);
    _paramProps.propSetInt(kOfxParamPropDisplayMax, ymax, 1);
  }

  void Int2DParamDescriptor::setDimensionLabels(const std::string& x, const std::string& y)
  {
    _paramProps.propSetString(kOfxParamPropDimensionLabel, x, 0, false);
    _paramProps.propSetString(kOfxParamPropDimensionLabel, y, 1, false);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // 3D int param descriptor

  /** @brief ctor */
  Int3DParamDescriptor::Int3DParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : ValueParamDescriptor(name, eInt3DParam, props)
  {
  }

  /** @brief set the default value, default is 0 */
  void 
    Int3DParamDescriptor::setDefault(int x, int y, int z)
  {
    _paramProps.propSetInt(kOfxParamPropDefault, x, 0);
    _paramProps.propSetInt(kOfxParamPropDefault, y, 1);
    _paramProps.propSetInt(kOfxParamPropDefault, z, 2);
  }

  /** @brief set the hard min/max range, default is INT_MIN, INT_MAX */
  void 
    Int3DParamDescriptor::setRange(int xmin, int ymin, int zmin,
    int xmax, int ymax, int zmax)
  {
    _paramProps.propSetInt(kOfxParamPropMin, xmin, 0);
    _paramProps.propSetInt(kOfxParamPropMin, ymin, 1);
    _paramProps.propSetInt(kOfxParamPropMin, zmin, 2);
    _paramProps.propSetInt(kOfxParamPropMax, xmax, 0);
    _paramProps.propSetInt(kOfxParamPropMax, ymax, 1);
    _paramProps.propSetInt(kOfxParamPropMax, zmax, 2);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    Int3DParamDescriptor::setDisplayRange(int xmin, int ymin, int zmin,
    int xmax, int ymax, int zmax)
  {
    _paramProps.propSetInt(kOfxParamPropDisplayMin, xmin, 0);
    _paramProps.propSetInt(kOfxParamPropDisplayMin, ymin, 1);
    _paramProps.propSetInt(kOfxParamPropDisplayMin, zmin, 2);
    _paramProps.propSetInt(kOfxParamPropDisplayMax, xmax, 0);
    _paramProps.propSetInt(kOfxParamPropDisplayMax, ymax, 1);
    _paramProps.propSetInt(kOfxParamPropDisplayMax, zmax, 2);
  }

  void Int3DParamDescriptor::setDimensionLabels(const std::string& x, const std::string& y, const std::string& z)
  {
    _paramProps.propSetString(kOfxParamPropDimensionLabel, x, 0, false);
    _paramProps.propSetString(kOfxParamPropDimensionLabel, y, 1, false);
    _paramProps.propSetString(kOfxParamPropDimensionLabel, z, 2, false);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // base class for all double param descriptors

  /** @brief hidden constructor */
  BaseDoubleParamDescriptor::BaseDoubleParamDescriptor(const std::string &name, ParamTypeEnum type, OfxPropertySetHandle props)
    : ValueParamDescriptor(name, type, props)
  {
  }

  /** @brief set the type of the double param, defaults to eDoubleTypePlain */
  void BaseDoubleParamDescriptor::setDoubleType(DoubleTypeEnum v)
  {
    switch(v) 
    {
    case eDoubleTypePlain :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypePlain);
      break;
    case eDoubleTypeAngle :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeAngle);
      break;
    case eDoubleTypeScale :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeScale);
      break;
    case eDoubleTypeTime :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeTime);
      break;
    case eDoubleTypeAbsoluteTime :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeAbsoluteTime);
      break;
    case eDoubleTypeX :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeX);
      break;
    case eDoubleTypeXAbsolute :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeXAbsolute);
      break;
    case eDoubleTypeY :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeY);
      break;
    case eDoubleTypeYAbsolute :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeYAbsolute);
      break;
    case eDoubleTypeXY :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeXY);
      break;
    case eDoubleTypeXYAbsolute :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeXYAbsolute);
      break;
#ifdef kOfxParamDoubleTypeNormalisedX
    case eDoubleTypeNormalisedX :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeNormalisedX);
      break;
    case eDoubleTypeNormalisedY :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeNormalisedY);
      break;
    case eDoubleTypeNormalisedXAbsolute :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeNormalisedXAbsolute);
      break;
    case eDoubleTypeNormalisedYAbsolute :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeNormalisedYAbsolute);
      break;
    case eDoubleTypeNormalisedXY :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeNormalisedXY);
      break;
    case eDoubleTypeNormalisedXYAbsolute :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeNormalisedXYAbsolute);
      break;
#endif
#ifdef OFX_EXTENSIONS_VEGAS
    case eDoubleTypePolar :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypePolar);
      break;
    case eDoubleTypeChrominance :
      _paramProps.propSetString(kOfxParamPropDoubleType, kOfxParamDoubleTypeChrominance);
      break;
#endif
    }
  }

  /** @brief returns true if the param descriptor prop kOfxParamPropDefaultCoordinateSystem is present */
  bool BaseDoubleParamDescriptor::supportsDefaultCoordinateSystem()
  {
    //OfxStatus stat = Private::gPropSuite->propSetString(_paramProps.propSetHandle(), kOfxParamPropDefaultCoordinateSystem, 0, kOfxParamCoordinatesCanonical);
    char *value;
    OfxStatus stat = Private::gPropSuite->propGetString(_paramProps.propSetHandle(), kOfxParamPropDefaultCoordinateSystem, 0, &value);
    return (stat == kOfxStatOK);
  }

  /** @brief set the type of coordinate system for default values */
  void BaseDoubleParamDescriptor::setDefaultCoordinateSystem(DefaultCoordinateSystemEnum v)
  {
    // this property was introduced with OpenFX 1.2
    switch(v)
    {
    case eCoordinatesCanonical :
      _paramProps.propSetString(kOfxParamPropDefaultCoordinateSystem, kOfxParamCoordinatesCanonical, false);
      break;
    case eCoordinatesNormalised :
      _paramProps.propSetString(kOfxParamPropDefaultCoordinateSystem, kOfxParamCoordinatesNormalised, false);
      break;
    }
  }

  /** @brief set the sensitivity of any gui slider */
  void BaseDoubleParamDescriptor::setIncrement(double v)
  {
    _paramProps.propSetDouble(kOfxParamPropIncrement, v);
  }

  /** @brief set the number of digits printed after a decimal point in any gui */
  void BaseDoubleParamDescriptor::setDigits(int v)
  {
    _paramProps.propSetInt(kOfxParamPropDigits, v);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // double param descriptor

  /** @brief ctor */
  DoubleParamDescriptor::DoubleParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : BaseDoubleParamDescriptor(name, eDoubleParam, props)
  {
  }

  /** @brief set the default value, default is 0 */
  void 
    DoubleParamDescriptor::setDefault(double v)
  {
    _paramProps.propSetDouble(kOfxParamPropDefault, v);
  }

  /** @brief set the hard min/max range, default is DOUBLE_MIN, DOUBLE_MAX */
  void 
    DoubleParamDescriptor::setRange(double min, double max)
  {
    _paramProps.propSetDouble(kOfxParamPropMin, min);
    _paramProps.propSetDouble(kOfxParamPropMax, max);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    DoubleParamDescriptor::setDisplayRange(double min, double max)
  {
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, min);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, max);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // 2D double param descriptor

  /** @brief ctor */
  Double2DParamDescriptor::Double2DParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : BaseDoubleParamDescriptor(name, eDouble2DParam, props)
  {
  }

#ifdef OFX_EXTENSIONS_NATRON
  /**
   * @brief Set as the
   **/
  void Double2DParamDescriptor::setRectanglePart(RectangleParamPartEnum rectanglePart)
  {
    int i;
    switch (rectanglePart) {
      case eRectangleParamPartNone:
        i = 0;
        break;
      case eRectangleParamPartPosition:
        i = 1;
        break;
      case eRectangleParamPartSize:
        i = 2;
        break;
    }
    _paramProps.propSetInt(kNatronOfxParamPropTypeRectangle, i, 0, false);
  }
#endif
  
  /** @brief set the default value, default is 0 */
  void 
    Double2DParamDescriptor::setDefault(double x, double y)
  {
    _paramProps.propSetDouble(kOfxParamPropDefault, x, 0);
    _paramProps.propSetDouble(kOfxParamPropDefault, y, 1);
  }

  /** @brief set the hard min/max range, default is DOUBLE_MIN, DOUBLE_MAX */
  void 
    Double2DParamDescriptor::setRange(double xmin, double ymin,
    double xmax, double ymax)
  {
    _paramProps.propSetDouble(kOfxParamPropMin, xmin, 0);
    _paramProps.propSetDouble(kOfxParamPropMin, ymin, 1);
    _paramProps.propSetDouble(kOfxParamPropMax, xmax, 0);
    _paramProps.propSetDouble(kOfxParamPropMax, ymax, 1);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    Double2DParamDescriptor::setDisplayRange(double xmin, double ymin,
    double xmax, double ymax)
  {
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, xmin, 0);
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, ymin, 1);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, xmax, 0);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, ymax, 1);
  }

  void Double2DParamDescriptor::setDimensionLabels(const std::string& x, const std::string& y)
  {
    _paramProps.propSetString(kOfxParamPropDimensionLabel, x, 0);
    _paramProps.propSetString(kOfxParamPropDimensionLabel, y, 1);
  }

#ifdef OFX_EXTENSIONS_VEGAS
  /** @brief set the color wheel level value, default is 0.75 */
  void Double2DParamDescriptor::setColorWheelLevel(double x)
  {
    _paramProps.propSetDouble(kOfxParamPropColorWheelLevel, x, false);
  }
#endif

  ////////////////////////////////////////////////////////////////////////////////
  // 3D double param descriptor

  /** @brief ctor */
  Double3DParamDescriptor::Double3DParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : BaseDoubleParamDescriptor(name, eDouble3DParam, props)
  {
  }
  
#ifdef OFX_EXTENSIONS_NATRON
  /**
   * @brief Set the (1-based) matrix row
   **/
  void Double3DParamDescriptor::setMatrixRow(int rowIndex)
  {
    _paramProps.propSetInt(kNatronOfxParamPropDoubleTypeMatrix3x3, rowIndex, 0, false);
  }
#endif

  /** @brief set the default value, default is 0 */
  void 
    Double3DParamDescriptor::setDefault(double x, double y, double z)
  {
    _paramProps.propSetDouble(kOfxParamPropDefault, x, 0);
    _paramProps.propSetDouble(kOfxParamPropDefault, y, 1);
    _paramProps.propSetDouble(kOfxParamPropDefault, z, 2);
  }

  /** @brief set the hard min/max range, default is -DBL_MAX, DBL_MAX */
  void 
    Double3DParamDescriptor::setRange(double xmin, double ymin, double zmin,
    double xmax, double ymax, double zmax)
  {
    _paramProps.propSetDouble(kOfxParamPropMin, xmin, 0);
    _paramProps.propSetDouble(kOfxParamPropMin, ymin, 1);
    _paramProps.propSetDouble(kOfxParamPropMin, zmin, 2);
    _paramProps.propSetDouble(kOfxParamPropMax, xmax, 0);
    _paramProps.propSetDouble(kOfxParamPropMax, ymax, 1);
    _paramProps.propSetDouble(kOfxParamPropMax, zmax, 2);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    Double3DParamDescriptor::setDisplayRange(double xmin, double ymin, double zmin,
    double xmax, double ymax, double zmax)
  {
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, xmin, 0);
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, ymin, 1);
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, zmin, 2);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, xmax, 0);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, ymax, 1);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, zmax, 2);
  }

  void Double3DParamDescriptor::setDimensionLabels(const std::string& x, const std::string& y, const std::string& z)
  {
    _paramProps.propSetString(kOfxParamPropDimensionLabel, x, 0);
    _paramProps.propSetString(kOfxParamPropDimensionLabel, y, 1);
    _paramProps.propSetString(kOfxParamPropDimensionLabel, z, 2);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // RGB param descriptor

  /** @brief hidden constructor */
  RGBParamDescriptor::RGBParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : ValueParamDescriptor(name, eRGBParam, props)
  {
  }

  /** @brief set the default value */
  void  RGBParamDescriptor::setDefault(double r, double g, double b)
  {
    _paramProps.propSetDouble(kOfxParamPropDefault, r, 0);
    _paramProps.propSetDouble(kOfxParamPropDefault, g, 1);
    _paramProps.propSetDouble(kOfxParamPropDefault, b, 2);
  }

  /** @brief set the hard min/max range, default is 0., 1. */
  void 
    RGBParamDescriptor::setRange(double rmin, double gmin, double bmin,
    double rmax, double gmax, double bmax)
  {
    _paramProps.propSetDouble(kOfxParamPropMin, rmin, 0);
    _paramProps.propSetDouble(kOfxParamPropMin, gmin, 1);
    _paramProps.propSetDouble(kOfxParamPropMin, bmin, 2);
    _paramProps.propSetDouble(kOfxParamPropMax, rmax, 0);
    _paramProps.propSetDouble(kOfxParamPropMax, gmax, 1);
    _paramProps.propSetDouble(kOfxParamPropMax, bmax, 2);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    RGBParamDescriptor::setDisplayRange(double rmin, double gmin, double bmin,
    double rmax, double gmax, double bmax)
  {
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, rmin, 0);
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, gmin, 1);
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, bmin, 2);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, rmax, 0);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, gmax, 1);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, bmax, 2);
  }

  void RGBParamDescriptor::setDimensionLabels(const std::string& r, const std::string& g, const std::string& b)
  {
    _paramProps.propSetString(kOfxParamPropDimensionLabel, r, 0);
    _paramProps.propSetString(kOfxParamPropDimensionLabel, g, 1);
    _paramProps.propSetString(kOfxParamPropDimensionLabel, b, 2);
  }

#ifdef OFX_EXTENSIONS_VEGAS
  /** @brief set the default UI color space of the RGB param, defaults to eColorSpaceHSV */
  void RGBParamDescriptor::setDefaultColorSpace(ColorSpaceEnum v)
  {
    switch(v) 
    {
    case eColorSpaceRGB :
      _paramProps.propSetString(kOfxParamColorDefaultColorspace, kOfxParamColorColorspaceRGB, false);
      break;
    case eColorSpaceHSV :
      _paramProps.propSetString(kOfxParamColorDefaultColorspace, kOfxParamColorColorspaceHSV, false);
      break;
    case eColorSpaceHSL :
      _paramProps.propSetString(kOfxParamColorDefaultColorspace, kOfxParamColorColorspaceHSL, false);
      break;
    case eColorSpaceLab :
      _paramProps.propSetString(kOfxParamColorDefaultColorspace, kOfxParamColorColorspaceLab, false);
      break;
    }
  }
#endif

  ////////////////////////////////////////////////////////////////////////////////
  // RGBA param descriptor

  /** @brief hidden constructor */
  RGBAParamDescriptor::RGBAParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : ValueParamDescriptor(name, eRGBAParam, props)
  {
  }

  /** @brief set the default value */
  void  RGBAParamDescriptor::setDefault(double r, double g, double b, double a)
  {
    _paramProps.propSetDouble(kOfxParamPropDefault, r, 0);
    _paramProps.propSetDouble(kOfxParamPropDefault, g, 1);
    _paramProps.propSetDouble(kOfxParamPropDefault, b, 2);
    _paramProps.propSetDouble(kOfxParamPropDefault, a, 3);
  }


  /** @brief set the hard min/max range, default is 0., 1. */
  void 
    RGBAParamDescriptor::setRange(double rmin, double gmin, double bmin, double amin,
    double rmax, double gmax, double bmax, double amax)
  {
    _paramProps.propSetDouble(kOfxParamPropMin, rmin, 0);
    _paramProps.propSetDouble(kOfxParamPropMin, gmin, 1);
    _paramProps.propSetDouble(kOfxParamPropMin, bmin, 2);
    _paramProps.propSetDouble(kOfxParamPropMin, amin, 3);
    _paramProps.propSetDouble(kOfxParamPropMax, rmax, 0);
    _paramProps.propSetDouble(kOfxParamPropMax, gmax, 1);
    _paramProps.propSetDouble(kOfxParamPropMax, bmax, 2);
    _paramProps.propSetDouble(kOfxParamPropMax, amax, 3);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    RGBAParamDescriptor::setDisplayRange(double rmin, double gmin, double bmin, double amin,
    double rmax, double gmax, double bmax, double amax)
  {
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, rmin, 0);
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, gmin, 1);
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, bmin, 2);
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, amin, 3);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, rmax, 0);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, gmax, 1);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, bmax, 2);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, amax, 3);
  }

  void RGBAParamDescriptor::setDimensionLabels(const std::string& r, const std::string& g, const std::string& b, const std::string& a)
  {
    _paramProps.propSetString(kOfxParamPropDimensionLabel, r, 0);
    _paramProps.propSetString(kOfxParamPropDimensionLabel, g, 1);
    _paramProps.propSetString(kOfxParamPropDimensionLabel, b, 2);
    _paramProps.propSetString(kOfxParamPropDimensionLabel, a, 3);
  }

#ifdef OFX_EXTENSIONS_VEGAS
  /** @brief set the default UI color space of the RGB param, defaults to eColorSpaceHSV */
  void RGBAParamDescriptor::setDefaultColorSpace(ColorSpaceEnum v)
  {
    switch(v) 
    {
    case eColorSpaceRGB :
      _paramProps.propSetString(kOfxParamColorDefaultColorspace, kOfxParamColorColorspaceRGB, false);
      break;
    case eColorSpaceHSV :
      _paramProps.propSetString(kOfxParamColorDefaultColorspace, kOfxParamColorColorspaceHSV, false);
      break;
    case eColorSpaceHSL :
      _paramProps.propSetString(kOfxParamColorDefaultColorspace, kOfxParamColorColorspaceHSL, false);
      break;
    case eColorSpaceLab :
      _paramProps.propSetString(kOfxParamColorDefaultColorspace, kOfxParamColorColorspaceLab, false);
      break;
    }
  }
#endif

  ////////////////////////////////////////////////////////////////////////////////
  // bool param descriptor

  /** @brief hidden constructor */
  BooleanParamDescriptor::BooleanParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : ValueParamDescriptor(name, eBooleanParam, props)
  {
  }

  /** @brief set the default value */
  void BooleanParamDescriptor::setDefault(bool v)
  {
    _paramProps.propSetInt(kOfxParamPropDefault, int(v));
  }

#ifdef OFX_EXTENSIONS_NATRON
  /*If called, the parameter will be defined as an action in the host viewport toolbar. The parameter should be added to a group on which the property kNatronOfxParamPropInViewerContextIsInToolbar was set to 1 */
  void BooleanParamDescriptor::setAsInViewportToolbuttonAction()
  {
    _paramProps.propSetInt(kNatronOfxParamPropInViewerContextIsInToolbar, 1);
  }

  void BooleanParamDescriptor::setAsToggableButton()
  {
    _paramProps.propSetInt(kNatronOfxBooleanParamPropIsToggableButton, 1);
  }

  /*If called, the host can add an entry in the shortcut editor for this parameter when the user is using the viewport.
   The default value for the shortcut may be specified on the ImageEffectDescriptor by calling the function setDefaultParamInViewportShortcut()*/
  void BooleanParamDescriptor::setInViewportCanHaveShortcut()
  {
    _paramProps.propSetInt(kNatronOfxParamPropInViewerContextCanHaveShortcut, 1);
  }

#endif
  ////////////////////////////////////////////////////////////////////////////////
  // choice param descriptor

  /** @brief hidden constructor */
  ChoiceParamDescriptor::ChoiceParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : ValueParamDescriptor(name, eChoiceParam, props)
  {
  }

  /** @brief set the default value */
  void ChoiceParamDescriptor::setDefault(int v)
  {
    _paramProps.propSetInt(kOfxParamPropDefault, v);
  }

  /** @brief how many options do we have */
  int ChoiceParamDescriptor::getNOptions(void)
  {
    int nCurrentValues = _paramProps.propGetDimension(kOfxParamPropChoiceOption);
    return nCurrentValues;
  }

  /** @brief set the default value */
  void ChoiceParamDescriptor::appendOption(const std::string &v, const std::string& label)
  {
    int nCurrentValues = _paramProps.propGetDimension(kOfxParamPropChoiceOption);
    if(!label.empty()) {
#ifdef OFX_EXTENSIONS_TUTTLE
      // Choice label is an ofx extension. If the host doesn't support it,
      // we put this information into the parameter hint.
      // from https://github.com/tuttleofx/TuttleOFX/commit/ae6e14e99f62b5efa89e4de4a3bc33129ac6afd0
      try {
        // this property is an optional extension.
         _paramProps.propSetString(kOfxParamPropChoiceLabelOption, label, nCurrentValues);
      } catch(std::exception&)
#endif
      {
        // If the kOfxParamPropChoiceLabelOption doesn't exist, we put that information into the Hint.
        // It's better than nothing...
        std::string hint = _paramProps.propGetString(kOfxParamPropHint);
        if(!hint.empty()) {
          hint += "\n";
          if( nCurrentValues == 0 ) {
            hint += "\n";
          }
        }
        hint += v + ": " + label;
        _paramProps.propSetString(kOfxParamPropHint, hint);
      }
    }
    _paramProps.propSetString(kOfxParamPropChoiceOption, v, nCurrentValues);
  }

  /** @brief set the default value */
  void ChoiceParamDescriptor::resetOptions(void)
  {
    _paramProps.propReset(kOfxParamPropChoiceOption);
#ifdef OFX_EXTENSIONS_TUTTLE
    if (_paramProps.propGetDimension(kOfxParamPropChoiceLabelOption, false) > 0) {
      _paramProps.propReset(kOfxParamPropChoiceLabelOption);
    }
#endif
  }

  /** @brief set to the default value */
  void ChoiceParamDescriptor::resetOptions(const std::vector<std::string>& newEntries,
                                           const std::vector<std::string>& newEntriesLabel)
  {
    assert(newEntries.size() == newEntriesLabel.size() || newEntriesLabel.empty());
    if (newEntries.empty() || (newEntries.size() != newEntriesLabel.size() && !newEntriesLabel.empty())) {

      // Invalid parameters or empty newEntries, reset the property
#ifdef OFX_EXTENSIONS_TUTTLE
      if (_paramProps.propGetDimension(kOfxParamPropChoiceLabelOption, false) > 0) {
        _paramProps.propReset(kOfxParamPropChoiceLabelOption);
      }
#endif
      _paramProps.propReset(kOfxParamPropChoiceOption);
    } else {
      // Set the new entries
#ifdef OFX_EXTENSIONS_TUTTLE
      if (!newEntriesLabel.empty()) {
        _paramProps.propSetStringN(kOfxParamPropChoiceLabelOption, newEntriesLabel, false);
      }
#endif
      _paramProps.propSetStringN(kOfxParamPropChoiceOption, newEntries);
    }

  }


#ifdef OFX_EXTENSIONS_NATRON
  /** @brief whether the menu should be cascading, and each option contains a slash-separated path to the item, defaults to false. */
  void ChoiceParamDescriptor::setCascading(const bool v)
  {
    _paramProps.propSetInt(kNatronOfxParamPropChoiceCascading, v, false);
  }

  /** @brief may the host add new options? */
  void ChoiceParamDescriptor::setHostCanAddOptions(bool can)
  {
    _paramProps.propSetInt(kNatronOfxParamPropChoiceHostCanAddOptions, (int)can, 0, false);
  }
#endif
  ////////////////////////////////////////////////////////////////////////////////
  // string param descriptor

  /** @brief hidden ctor */
  StringParamDescriptor::StringParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : ValueParamDescriptor(name, eStringParam, props)
  {
  }

  /** @brief set the default value, default is 0 */
  void StringParamDescriptor::setDefault(const std::string &v)
  {
    _paramProps.propSetString(kOfxParamPropDefault, v);
  }

  /** @brief sets the kind of the string param, defaults to eStringSingleLine */
  void StringParamDescriptor::setStringType(StringTypeEnum v)
  {
    switch (v) 
    {
    case eStringTypeSingleLine :
      _paramProps.propSetString(kOfxParamPropStringMode,  kOfxParamStringIsSingleLine);
      break;
    case eStringTypeMultiLine :
      _paramProps.propSetString(kOfxParamPropStringMode,  kOfxParamStringIsMultiLine);
      break;
    case eStringTypeFilePath :
      _paramProps.propSetString(kOfxParamPropStringMode,  kOfxParamStringIsFilePath);
      break;
    case eStringTypeDirectoryPath :
      _paramProps.propSetString(kOfxParamPropStringMode,  kOfxParamStringIsDirectoryPath);
      break;
    case eStringTypeLabel :
      _paramProps.propSetString(kOfxParamPropStringMode,  kOfxParamStringIsLabel);
      break;
    case eStringTypeRichTextFormat :
      _paramProps.propSetString(kOfxParamPropStringMode,  kOfxParamStringIsRichTextFormat);
      break;
    }
  }

  /** @brief if the string param is a file path, say that we are picking an existing file, defaults to true */
  void StringParamDescriptor::setFilePathExists(bool v)
  {
    _paramProps.propSetInt(kOfxParamPropStringFilePathExists, int(v));
  }

  ////////////////////////////////////////////////////////////////////////////////
  // custom param descriptor

  /** @brief hidden ctor */
  CustomParamDescriptor::CustomParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : ValueParamDescriptor(name, eCustomParam, props)
  {
  }

  /** @brief set the default value, default is 0 */
  void CustomParamDescriptor::setDefault(const std::string &v)
  {
    _paramProps.propSetString(kOfxParamPropDefault, v);
  }

  void CustomParamDescriptor::setCustomInterpolation(bool v)
  {
    _paramProps.propSetPointer(kOfxParamPropCustomInterpCallbackV1, v ? (void*)OFX::Private::customParamInterpolationV1Entry : NULL);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // group param descriptor

  /** @brief hidden constructor */
  GroupParamDescriptor::GroupParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : ParamDescriptor(name, eGroupParam, props)
  {
  }

  /** @brief whether the initial state of a group is open or closed in a hierarchical layout, defaults to true */
  void GroupParamDescriptor::setOpen(const bool v)
  {
    _paramProps.propSetInt(kOfxParamPropGroupOpen, v, false); // introduced in OFX 1.2
  }

#ifdef OFX_EXTENSIONS_NUKE
  void GroupParamDescriptor::setAsTab()
  {
    _paramProps.propSetInt(kFnOfxParamPropGroupIsTab, 1, false); // Nuke extension
  }
#endif

#ifdef OFX_EXTENSIONS_NATRON
  void GroupParamDescriptor::setAsDialog()
  {
    _paramProps.propSetInt(kNatronOfxGroupParamPropIsDialog, 1, false);
  }

  void
    GroupParamDescriptor::setAsInViewportToolbutton(bool canHaveShortcut)
  {
    _paramProps.propSetInt(kNatronOfxParamPropInViewerContextIsInToolbar, 1, false);
    _paramProps.propSetInt(kNatronOfxParamPropInViewerContextCanHaveShortcut, (int)canHaveShortcut, false);
  }
#endif
  ////////////////////////////////////////////////////////////////////////////////
  // page param descriptor

  /** @brief hidden constructor */
  PageParamDescriptor::PageParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : ParamDescriptor(name, ePageParam, props)
  {
  }

#ifdef OFX_EXTENSIONS_NATRON

  void PageParamDescriptor::setAsInViewportToolbar()
  {
    _paramProps.propSetInt(kNatronOfxParamPropInViewerContextIsInToolbar, 1, 0);
  }
#endif

  /** @brief adds a child parameter. Note the two existing pseudo params, gColumnSkip  and gRowSkip */
  void PageParamDescriptor::addChild(const ParamDescriptor &p)
  {
    int nKids = _paramProps.propGetDimension(kOfxParamPropPageChild);
    _paramProps.propSetString(kOfxParamPropPageChild, p.getName(), nKids);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // pushbutton param descriptor

  /** @brief hidden constructor */
  PushButtonParamDescriptor::PushButtonParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : ParamDescriptor(name, ePushButtonParam, props)
  {
  }

#ifdef OFX_EXTENSIONS_NATRON
  /*If called, the host can add an entry in the shortcut editor for this parameter when the user is using the viewport.
   The default value for the shortcut may be specified on the ImageEffectDescriptor by calling the function setDefaultParamInViewportShortcut()*/
  void PushButtonParamDescriptor::setInViewportCanHaveShortcut()
  {
    _paramProps.propSetInt(kNatronOfxParamPropInViewerContextCanHaveShortcut, 1, 0);
  }
#endif

  ////////////////////////////////////////////////////////////////////////////////
  // parametric param descriptor

  /** @brief hidden constructor */
  ParametricParamDescriptor::ParametricParamDescriptor(const std::string &name, OfxPropertySetHandle props)
    : ParamDescriptor(name, eParametricParam, props)
    , _ofxParamHandle(0)
    , _paramSet(0)
  {
  }

  void ParametricParamDescriptor::setParamSet(ParamSetDescriptor& paramSet)
  {
    _paramSet = &paramSet;
    OfxStatus stat = OFX::Private::gParamSuite->paramGetHandle(_paramSet->getParamSetHandle(), getName().c_str(), &_ofxParamHandle, 0);
    throwSuiteStatusException(stat);
  }

  void ParametricParamDescriptor::setRange(const double min, const double max)
  {
    _paramProps.propSetDouble(kOfxParamPropParametricRange, min, 0);
    _paramProps.propSetDouble(kOfxParamPropParametricRange, max, 1);
  }

  void ParametricParamDescriptor::setDimension(const int dimension)
  {
    _paramProps.propSetInt(kOfxParamPropParametricDimension, dimension);
  }

  void ParametricParamDescriptor::setDimensionLabel(const std::string& label, const int id)
  {
    _paramProps.propSetString(kOfxParamPropDimensionLabel, label, id);
  }

  void ParametricParamDescriptor::setUIColour(const int id, const OfxRGBColourD& color)
  {
    _paramProps.propSetDouble(kOfxParamPropParametricUIColour, color.r, id*3 + 0);
    _paramProps.propSetDouble(kOfxParamPropParametricUIColour, color.g, id*3 + 1);
    _paramProps.propSetDouble(kOfxParamPropParametricUIColour, color.b, id*3 + 2);
  }

  void ParametricParamDescriptor::addControlPoint(const int id, const OfxTime time, const double x, const double y, const bool addKey)
  {
    if (time != time) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParametricParameterSuite->parametricParamAddControlPoint(_ofxParamHandle, id, time, x, y, addKey);
    throwSuiteStatusException(stat);
  }

  void ParametricParamDescriptor::setIdentity(const int id)
  {
    addControlPoint(id, 0, 0, 0, false);
    addControlPoint(id, 0, 1, 1, false);
  }
    
  void ParametricParamDescriptor::setIdentity()
  {
    const int nbCurves = _paramProps.propGetInt(kOfxParamPropParametricDimension);
    for(int i = 0; i < nbCurves; ++i) {
      setIdentity(i);
    }
  }
    
  void ParametricParamDescriptor::setInteractDescriptor(ParamInteractDescriptor* desc)
  {
    _interact.reset(desc);
    _paramProps.propSetPointer(kOfxParamPropParametricInteractBackground, (void*)desc->getMainEntry());
    desc->setParamName(getName());
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Descriptor for a set of parameters
  /** @brief hidden ctor */
  ParamSetDescriptor::ParamSetDescriptor(void)
    : _paramSetHandle(0)
  {
  }

  ParamDescriptor* ParamSetDescriptor::getParamDescriptor(const std::string& name) const
  {
    std::map<std::string, ParamDescriptor*>::const_iterator it = _definedParams.find(name);
    if(it!=_definedParams.end())
      return it->second;
    return 0;
  }

  /** @brief set the param set handle */
  void
    ParamSetDescriptor::setParamSetHandle(OfxParamSetHandle h)
  {
    // set me handle
    _paramSetHandle = h;

    if(h) {
      // fetch me props
      OfxPropertySetHandle props;
      OfxStatus stat = OFX::Private::gParamSuite->paramSetGetPropertySet(h, &props);
      _paramSetProps.propSetHandle(props);
      throwSuiteStatusException(stat);
    }
    else {
      _paramSetProps.propSetHandle(0);
    }
  }

  /** @brief dtor */
  ParamSetDescriptor::~ParamSetDescriptor()
  {
    // delete any descriptor we may have constructed
    std::map<std::string, ParamDescriptor *>::iterator iter;
    for(iter = _definedParams.begin(); iter != _definedParams.end(); ++iter) {
      if(iter->second) {
        delete iter->second;
        iter->second = NULL;
      }
    }
  }

  /** @brief estabilishes the order of page params. Do it by calling it in turn for each page */
  void 
    ParamSetDescriptor::setPageParamOrder(PageParamDescriptor &p)
  {
    int nPages = _paramSetProps.propGetDimension(kOfxPluginPropParamPageOrder);
    _paramSetProps.propSetString(kOfxPluginPropParamPageOrder, p.getName().c_str(), nPages);
  }


  /** @brief calls the raw OFX routine to define a param */
  void ParamSetDescriptor::defineRawParam(const std::string &name, ParamTypeEnum paramType, OfxPropertySetHandle &props)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramDefine(_paramSetHandle, mapParamTypeEnumToString(paramType), name.c_str(), &props);
    throwSuiteStatusException(stat);
  }

  /** @brief if a param has been defined in this set, go find it */
  ParamDescriptor *
    ParamSetDescriptor::findPreviouslyDefinedParam(const std::string &name) const
  {
    // search
    std::map<std::string, ParamDescriptor *>::const_iterator search;
    search = _definedParams.find(name);
    if(search == _definedParams.end())
      return NULL;
    return search->second;
  }

  /** @brief Define an integer param, only callable from describe in context */
  IntParamDescriptor * 
    ParamSetDescriptor::defineIntParam(const std::string &name)
  { 
    IntParamDescriptor *param = NULL;
    defineParamDescriptor(name, eIntParam, param);
    return param;
  }

  /** @brief Define a 2D integer param */
  Int2DParamDescriptor *ParamSetDescriptor::defineInt2DParam(const std::string &name)
  {
    Int2DParamDescriptor *param = NULL;
    defineParamDescriptor(name, eInt2DParam, param);
    return param;
  }

  /** @brief Define a 3D integer param */
  Int3DParamDescriptor *ParamSetDescriptor::defineInt3DParam(const std::string &name)
  {
    Int3DParamDescriptor *param = NULL;
    defineParamDescriptor(name, eInt3DParam, param);
    return param;
  }

  /** @brief Define an double param, only callable from describe in context */
  DoubleParamDescriptor * 
    ParamSetDescriptor::defineDoubleParam(const std::string &name)
  { 
    DoubleParamDescriptor *param = NULL;
    defineParamDescriptor(name, eDoubleParam, param);
    return param;
  }

  /** @brief Define a 2D double param */
  Double2DParamDescriptor *ParamSetDescriptor::defineDouble2DParam(const std::string &name)
  {
    Double2DParamDescriptor *param = NULL;
    defineParamDescriptor(name, eDouble2DParam, param);
    return param;
  }

  /** @brief Define a 3D double param */
  Double3DParamDescriptor *ParamSetDescriptor::defineDouble3DParam(const std::string &name)
  {
    Double3DParamDescriptor *param = NULL;
    defineParamDescriptor(name, eDouble3DParam, param);
    return param;
  }

  /** @brief Define a string param */
  StringParamDescriptor *ParamSetDescriptor::defineStringParam(const std::string &name)
  {
    StringParamDescriptor *param = NULL;
    defineParamDescriptor(name, eStringParam, param);
    return param;
  }

  /** @brief Define a RGBA param */
  RGBAParamDescriptor *ParamSetDescriptor::defineRGBAParam(const std::string &name)
  {
    RGBAParamDescriptor *param = NULL;
    defineParamDescriptor(name, eRGBAParam, param);
    return param;
  }

  /** @brief Define an RGB  param */
  RGBParamDescriptor *ParamSetDescriptor::defineRGBParam(const std::string &name)
  {
    RGBParamDescriptor *param = NULL;
    defineParamDescriptor(name, eRGBParam, param);
    return param;
  }

  /** @brief Define a Boolean  param */
  BooleanParamDescriptor *ParamSetDescriptor::defineBooleanParam(const std::string &name)
  {
    BooleanParamDescriptor *param = NULL;
    defineParamDescriptor(name, eBooleanParam, param);
    return param;
  }

  /** @brief Define a Choice param */
  ChoiceParamDescriptor *ParamSetDescriptor::defineChoiceParam(const std::string &name)
  {
    ChoiceParamDescriptor *param = NULL;
    defineParamDescriptor(name, eChoiceParam, param);
    return param;
  }

  /** @brief Define a group param */
  GroupParamDescriptor *ParamSetDescriptor::defineGroupParam(const std::string &name)
  {
    GroupParamDescriptor *param = NULL;
    defineParamDescriptor(name, eGroupParam, param);
    return param;
  }

  /** @brief Define a Page param */
  PageParamDescriptor *ParamSetDescriptor::definePageParam(const std::string &name)
  {
    PageParamDescriptor *param = NULL;
    defineParamDescriptor(name, ePageParam, param);
    return param;
  }

  /** @brief Define a push button  param */
  PushButtonParamDescriptor *ParamSetDescriptor::definePushButtonParam(const std::string &name)
  {
    PushButtonParamDescriptor *param = NULL;
    defineParamDescriptor(name, ePushButtonParam, param);
    return param;
  }

  /** @brief Define a parametric param */
  ParametricParamDescriptor* ParamSetDescriptor::defineParametricParam(const std::string &name)
  {
    ParametricParamDescriptor* param = NULL;
    if (defineParamDescriptor(name, eParametricParam, param)) {
      // Parametric parameters need the ParamSet !
      param->setParamSet(*this);
    }
    return param;
  }

  /** @brief Define a custom param */
  CustomParamDescriptor *ParamSetDescriptor::defineCustomParam(const std::string &name)
  {
    CustomParamDescriptor *param = NULL;
    defineParamDescriptor(name, eCustomParam, param);
    return param;
  }

  /** @brief Fetch an integer param */
  IntParamDescriptor * 
    ParamSetDescriptor::fetchIntParam(const std::string &name) const
  { 
    IntParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eIntParam, param);
    return param;
  }

  /** @brief Fetch a 2D integer param */
  Int2DParamDescriptor *ParamSetDescriptor::fetchInt2DParam(const std::string &name) const
  {
    Int2DParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eInt2DParam, param);
    return param;
  }

  /** @brief Fetch a 3D integer param */
  Int3DParamDescriptor *ParamSetDescriptor::fetchInt3DParam(const std::string &name) const
  {
    Int3DParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eInt3DParam, param);
    return param;
  }

  /** @brief Fetch an double param */
  DoubleParamDescriptor *ParamSetDescriptor::fetchDoubleParam(const std::string &name) const
  { 
    DoubleParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eDoubleParam, param);
    return param;
  }

  /** @brief Fetch a 2D double param */
  Double2DParamDescriptor *ParamSetDescriptor::fetchDouble2DParam(const std::string &name) const
  {
    Double2DParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eDouble2DParam, param);
    return param;
  }

  /** @brief Fetch a 3D double param */
  Double3DParamDescriptor *ParamSetDescriptor::fetchDouble3DParam(const std::string &name) const
  {
    Double3DParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eDouble3DParam, param);
    return param;
  }

  /** @brief Fetch a string param */
  StringParamDescriptor *ParamSetDescriptor::fetchStringParam(const std::string &name) const
  {
    StringParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eStringParam, param);
    return param;
  }

  /** @brief Fetch a RGBA param */
  RGBAParamDescriptor *ParamSetDescriptor::fetchRGBAParam(const std::string &name) const
  {
    RGBAParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eRGBAParam, param);
    return param;
  }

  /** @brief Fetch an RGB  param */
  RGBParamDescriptor *ParamSetDescriptor::fetchRGBParam(const std::string &name) const
  {
    RGBParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eRGBParam, param);
    return param;
  }

  /** @brief Fetch a Boolean  param */
  BooleanParamDescriptor *ParamSetDescriptor::fetchBooleanParam(const std::string &name) const
  {
    BooleanParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eBooleanParam, param);
    return param;
  }

  /** @brief Fetch a Choice param */
  ChoiceParamDescriptor *ParamSetDescriptor::fetchChoiceParam(const std::string &name) const
  {
    ChoiceParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eChoiceParam, param);
    return param;
  }

  /** @brief Fetch a group param */
  GroupParamDescriptor *ParamSetDescriptor::fetchGroupParam(const std::string &name) const
  {
    GroupParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eGroupParam, param);
    return param;
  }

  /** @brief Fetch a Page param */
  PageParamDescriptor *ParamSetDescriptor::fetchPageParam(const std::string &name) const
  {
    PageParamDescriptor *param = NULL;
    fetchParamDescriptor(name, ePageParam, param);
    return param;
  }

  /** @brief Fetch a push button  param */
  PushButtonParamDescriptor *ParamSetDescriptor::fetchPushButtonParam(const std::string &name) const
  {
    PushButtonParamDescriptor *param = NULL;
    fetchParamDescriptor(name, ePushButtonParam, param);
    return param;
  }

  /** @brief Fetch a custom param */
  CustomParamDescriptor *ParamSetDescriptor::fetchCustomParam(const std::string &name) const
  {
    CustomParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eCustomParam, param);
    return param;
  }

  /** @brief Fetch a parametric param */
  ParametricParamDescriptor *ParamSetDescriptor::fetchParametricParam(const std::string &name) const
  {
    ParametricParamDescriptor *param = NULL;
    fetchParamDescriptor(name, eParametricParam, param);
    return param;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /** @brief Base class for all param instances */
  Param::Param(const ParamSet *paramSet, const std::string &name, ParamTypeEnum type, OfxParamHandle handle)
    : _paramSet(paramSet)
    , _paramName(name)
    , _paramType(type)
    , _paramHandle(handle)      
  {
    // fetch our property handle
    OfxPropertySetHandle propHandle;
    OfxStatus stat = OFX::Private::gParamSuite->paramGetPropertySet(handle, &propHandle);
    throwSuiteStatusException(stat);
    _paramProps.propSetHandle(propHandle);

    // and validate the properties
    OFX::Validation::validateParameterProperties(type, _paramProps, false);
  }

  /** @brief dtor */
  Param::~Param()
  {
  }

  /** @brief get name */
  const std::string &Param::getName(void) const
  {
    return _paramName;
  }

  /** @brief, set the label properties in a param */
  void Param::setLabel(const std::string &label)
  {
    _paramProps.propSetString(kOfxPropLabel, label);
  }

  /** @brief, set the label properties in a param */
  void Param::setLabels(const std::string &label, const std::string &shortLabel, const std::string &longLabel)
  {
    setLabel(label);
    _paramProps.propSetString(kOfxPropShortLabel, shortLabel, false);
    _paramProps.propSetString(kOfxPropLongLabel, longLabel, false);
  }

  /** @brief set the secretness of the param, defaults to false */
  void Param::setIsSecret(bool v)
  {
    _paramProps.propSetInt(kOfxParamPropSecret, v);
  }

  /** @brief set the param hint */
  void Param::setHint(const std::string &v)
  {
    _paramProps.propSetString(kOfxParamPropHint, v, false);
  }

  /** @brief whether the param is enabled */
  void Param::setEnabled(bool v)
  {
    _paramProps.propSetInt(kOfxParamPropEnabled, v);
  }

  /** @brief set the private data pointer */
  void Param::setDataPtr(void* ptr)
  {
    _paramProps.propSetPointer(kOfxParamPropDataPtr, ptr);
  }

  /** @brief fetch the label */
  void Param::getLabel(std::string &label) const
  {
    label      = _paramProps.propGetString(kOfxPropLabel);
  }

  /** @brief fetch the labels */
  void Param::getLabels(std::string &label, std::string &shortLabel, std::string &longLabel) const
  {
    getLabel(label);
    shortLabel = _paramProps.propGetString(kOfxPropShortLabel, false);
    longLabel  = _paramProps.propGetString(kOfxPropLongLabel, false);
  }

  /** @brief get whether the param is secret */
  bool Param::getIsSecret(void) const
  {
    bool v = _paramProps.propGetInt(kOfxParamPropSecret) != 0;
    return v;
  }

  /** @brief whether the param is enabled */
  bool Param::getIsEnable(void) const
  {
    bool v = _paramProps.propGetInt(kOfxParamPropEnabled) != 0;
    return v;
  }

  /** @brief get the private data pointer */
  void* Param::getDataPtr(void) const
  {
    return   _paramProps.propGetPointer(kOfxParamPropDataPtr);
  }

  /** @brief get the param hint */
  std::string Param::getHint(void) const
  {
    std::string v  = _paramProps.propGetString(kOfxParamPropHint, false);
    return v;
  }

  /** @brief get the script name */
  std::string Param::getScriptName(void) const
  {
    std::string v  = _paramProps.propGetString(kOfxParamPropScriptName, false);
    return v;
  }

  /** @brief get the group param that is the parent of this one */
  GroupParam *Param::getParent(void) const
  {
    std::string v  = _paramProps.propGetString(kOfxParamPropParent);
    if(v == "") return NULL;
    return _paramSet->fetchGroupParam(v);
  }

  /** @brief get the icon file name (SVG or PNG) */
  std::string Param::getIcon(bool pngFormat) const
  {
    std::string v  = _paramProps.propGetString(kOfxPropIcon, (int)pngFormat, false); // OFX 1.2
    return v;
  }
    
  bool Param::getHostHasNativeOverlayHandle() const
  {
    bool v = _paramProps.propGetInt(kOfxParamPropHasHostOverlayHandle, 0, false) != 0; // OFX 1.2
    return v;
  }

#ifdef OFX_EXTENSIONS_NATRON
  /*Set the label of the parameter that should be displayed on the host viewport*/
  void Param::setInViewportLabel(const std::string& label)
  {
    _paramProps.propSetString(kNatronOfxParamPropInViewerContextLabel, label, false);
  }

  /*Set whether the parameter should be secret or not in the host viewport. This is only relevant if the parameter has been added to the kNatronOfxImageEffectPropInViewerContextParamsOrder property on the image effect descriptor.*/
  void Param::setInViewportIsSecret(bool secret)
  {
    _paramProps.propSetInt(kNatronOfxParamPropInViewerContextSecret, (int)secret, false);
  }

#endif

  ////////////////////////////////////////////////////////////////////////////////
  /** @brief Wraps up a value holding param */

  /** @brief hidden constructor */
  ValueParam::ValueParam(const ParamSet *paramSet, const std::string &name, ParamTypeEnum type, OfxParamHandle handle)
    : Param(paramSet, name, type, handle)
  {
  }

  /** @brief dtor */
  ValueParam::~ValueParam()
  {
  }

  /** @brief Set's whether the value of the param is significant (ie: affects the rendered image) */
  void 
    ValueParam::setEvaluateOnChange(bool v)
  {
    _paramProps.propSetInt(kOfxParamPropEvaluateOnChange, v);
  }

  /** @brief is the param animating */
  bool 
    ValueParam::getIsAnimating(void) const
  {
    return _paramProps.propGetInt(kOfxParamPropIsAnimating) != 0;
  }

  /** @brief is the param auto keing */
  bool 
    ValueParam::getIsAutoKeying(void) const
  {
    return _paramProps.propGetInt(kOfxParamPropIsAutoKeying) != 0;
  }

  /** @brief is the param persistent */
  bool 
    ValueParam::getIsPersistent(void) const
  {
    return _paramProps.propGetInt(kOfxParamPropPersistent) != 0;
  }

  /** @brief Get's whether the value of the param is significant (ie: affects the rendered image) */
  bool 
    ValueParam::getEvaluateOnChange(void) const
  {
    return _paramProps.propGetInt(kOfxParamPropEvaluateOnChange) != 0;
  }

  /** @brief Get's whether the value of the param is significant (ie: affects the rendered image) */
  CacheInvalidationEnum 
    ValueParam::getCacheInvalidation(void) const
  {
    std::string v = _paramProps.propGetString(kOfxParamPropCacheInvalidation);
    if(v == kOfxParamInvalidateValueChange)
      return eCacheInvalidateValueChange;
    else if(v == kOfxParamInvalidateValueChangeToEnd)
      return eCacheInvalidateValueChangeToEnd;
    else // if(v == kOfxParamInvalidateAll)
      return eCacheInvalidateValueAll;
  }

  /** @brief if the param is animating, the number of keys in it, otherwise 0 */
  unsigned int 
    ValueParam::getNumKeys(void)
  {
    if(!OFX::Private::gParamSuite->paramGetNumKeys) throwHostMissingSuiteException("paramGetNumKeys");
    unsigned int v = 0;
    OfxStatus stat = OFX::Private::gParamSuite->paramGetNumKeys(_paramHandle, &v);
    throwSuiteStatusException(stat);
    return v;
  }

  /** @brief get the time of the nth key, nth must be between 0 and getNumKeys-1 */
  double 
    ValueParam::getKeyTime(int nthKey) throw(OFX::Exception::Suite, std::out_of_range)
  {
    if(!OFX::Private::gParamSuite->paramGetKeyTime) throwHostMissingSuiteException("paramGetKeyTime");
    double v = 0;
    OfxStatus stat = OFX::Private::gParamSuite->paramGetKeyTime(_paramHandle, nthKey, &v);

    // oops?
    if(stat == kOfxStatFailed) throw std::out_of_range("ValueParam::getKeyTime key index out of range");
    throwSuiteStatusException(stat); 
    return v;
  }

  /** @brief find the index of a key by a time */
  int 
    ValueParam::getKeyIndex(double time, 
    KeySearchEnum searchDir)
  {
    if (time != time) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramGetKeyIndex) throwHostMissingSuiteException("paramGetKeyIndex");
    int v = 0;

    // turn enum into -1,0,1
    int dir = searchDir == eKeySearchBackwards ? -1 : (searchDir == eKeySearchNear ? 0 : 1);

    // call raw param function
    OfxStatus stat = OFX::Private::gParamSuite->paramGetKeyIndex(_paramHandle, time, dir, &v);

    // oops?
    if(stat == kOfxStatFailed) return -1; // if search failed, return -1
    throwSuiteStatusException(stat); 
    return v;
  }

  /** @brief deletes a key at the given time */
  void 
    ValueParam::deleteKeyAtTime(double time)
  {
    if (time != time) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramDeleteKey) throwHostMissingSuiteException("paramDeleteKey");
    OfxStatus stat = OFX::Private::gParamSuite->paramDeleteKey(_paramHandle, time);
    if(stat == kOfxStatFailed) return; // if no key at time, fail quietly
    throwSuiteStatusException(stat); 
  }

  /** @brief delete all the keys */
  void 
    ValueParam::deleteAllKeys(void)
  { 
    if(!OFX::Private::gParamSuite->paramDeleteAllKeys) throwHostMissingSuiteException("paramDeleteAllKeys");
    OfxStatus stat = OFX::Private::gParamSuite->paramDeleteAllKeys(_paramHandle);
    throwSuiteStatusException(stat); 
  }

  /** @brief copy parameter from another, including any animation etc... */
  void ValueParam::copyFrom(const ValueParam& from, OfxTime dstOffset, const OfxRangeD *frameRange)
  {
    if(!OFX::Private::gParamSuite->paramCopy) throwHostMissingSuiteException("paramCopy");
    OfxStatus stat = OFX::Private::gParamSuite->paramCopy(_paramHandle, from._paramHandle, dstOffset, frameRange);
    throwSuiteStatusException(stat);
  }

#ifdef OFX_EXTENSIONS_VEGAS
  /** @brief gets the interpolation type of a key at the given time */
  VegasInterpolationEnum ValueParam::getKeyInterpolation(double time)
  {
    if (time != time) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gVegasKeyframeSuite) throwHostMissingSuiteException("vegasKeyframeSuite");
    char *cStr;
    OfxStatus stat = OFX::Private::gVegasKeyframeSuite->paramGetKeyInterpolation(_paramHandle, time, &cStr);
    throwSuiteStatusException(stat); 
    return mapToInterpolationEnum(cStr);
  }
  
  /** @brief sets the interpolation type of a key at the given time */
  void ValueParam::setKeyInterpolation(double time, VegasInterpolationEnum interpolation)
  {
    if (time != time) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gVegasKeyframeSuite) throwHostMissingSuiteException("vegasKeyframeSuite");
    OfxStatus stat = OFX::Private::gVegasKeyframeSuite->paramSetKeyInterpolation(_paramHandle, time, mapToInterpolationTypeEnum(interpolation));
    throwSuiteStatusException(stat); 
  }
#endif

  ////////////////////////////////////////////////////////////////////////////////
  // Wraps up an integer param */

  /** @brief hidden constructor */
  IntParam::IntParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : ValueParam(paramSet, name, eIntParam, handle)
  {
  }

  /** @brief set the default value */
  void IntParam::setDefault(int v)
  {
    _paramProps.propSetInt(kOfxParamPropDefault, v);
  }

  /** @brief set the hard min/max range, default is INT_MIN, INT_MAX */
  void IntParam::setRange(int min, int max)
  {
    _paramProps.propSetInt(kOfxParamPropMin, min);
    _paramProps.propSetInt(kOfxParamPropMax, max);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void IntParam::setDisplayRange(int min, int max)
  {
    _paramProps.propSetInt(kOfxParamPropDisplayMin, min);
    _paramProps.propSetInt(kOfxParamPropDisplayMax, max);
  }

  /** @brief get the default value */
  void IntParam::getDefault(int &v)
  {    
    v = _paramProps.propGetInt(kOfxParamPropDefault);
  }

  /** @brief set the hard min/max range, default is INT_MIN, INT_MAX */
  void IntParam::getRange(int &min, int &max)
  {
    min = _paramProps.propGetInt(kOfxParamPropMin);
    max = _paramProps.propGetInt(kOfxParamPropMax);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void IntParam::getDisplayRange(int &min, int &max)
  {
    min = _paramProps.propGetInt(kOfxParamPropDisplayMin);
    max = _paramProps.propGetInt(kOfxParamPropDisplayMax);
  }

  /** @brief get value */
  void IntParam::getValue(int &v)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValue(_paramHandle, &v);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void IntParam::getValueAtTime(double t, int &v)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValueAtTime(_paramHandle, t, &v);
    throwSuiteStatusException(stat);
  }

  /** @brief set value */
  void IntParam::setValue(int v)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValue(_paramHandle, v);
    throwSuiteStatusException(stat);
  }

  /** @brief set the value at a time, implicitly adds a keyframe */
  void IntParam::setValueAtTime(double t, int v)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramSetValueAtTime) throwHostMissingSuiteException("paramSetValueAtTime");
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValueAtTime(_paramHandle, t, v);
    throwSuiteStatusException(stat);
  }

  /** @brief delete all keys and set to default value */
  void IntParam::resetToDefault()
  {
    deleteAllKeys();
    int v;
    getDefault(v);
    setValue(v);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // 2D Int params

  /** @brief hidden constructor */
  Int2DParam::Int2DParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : ValueParam(paramSet, name, eInt2DParam, handle)
  {
  }

  /** @brief set the default value */
  void Int2DParam::setDefault(int x, int y)
  {
    _paramProps.propSetInt(kOfxParamPropDefault, x, 0);
    _paramProps.propSetInt(kOfxParamPropDefault, y, 1);
  }

  /** @brief set the hard min/max range, default is INT_MIN, INT_MAX */
  void 
    Int2DParam::setRange(int xmin, int ymin,
    int xmax, int ymax)
  {
    _paramProps.propSetInt(kOfxParamPropMin, xmin, 0);
    _paramProps.propSetInt(kOfxParamPropMin, ymin, 1);
    _paramProps.propSetInt(kOfxParamPropMax, xmax, 0);
    _paramProps.propSetInt(kOfxParamPropMax, ymax, 1);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    Int2DParam::setDisplayRange(int xmin, int ymin,
    int xmax, int ymax)
  {
    _paramProps.propSetInt(kOfxParamPropDisplayMin, xmin, 0);
    _paramProps.propSetInt(kOfxParamPropDisplayMin, ymin, 1);
    _paramProps.propSetInt(kOfxParamPropDisplayMax, xmax, 0);
    _paramProps.propSetInt(kOfxParamPropDisplayMax, ymax, 1);
  }

  /** @brief het the default value */
  void Int2DParam::getDefault(int &x, int &y)
  {    
    x = _paramProps.propGetInt(kOfxParamPropDefault, 0);
    y = _paramProps.propGetInt(kOfxParamPropDefault, 1);
  }


  /** @brief set the hard min/max range, default is INT_MIN, INT_MAX */
  void 
    Int2DParam::getRange(int &xmin, int &ymin,
    int &xmax, int &ymax)
  {
    xmin = _paramProps.propGetInt(kOfxParamPropMin, 0);
    ymin = _paramProps.propGetInt(kOfxParamPropMin, 1);
    xmax = _paramProps.propGetInt(kOfxParamPropMax, 0);
    ymax = _paramProps.propGetInt(kOfxParamPropMax, 1);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    Int2DParam::getDisplayRange(int &xmin, int &ymin,
    int &xmax, int &ymax)
  {
    xmin = _paramProps.propGetInt(kOfxParamPropDisplayMin, 0);
    ymin = _paramProps.propGetInt(kOfxParamPropDisplayMin, 1);
    xmax = _paramProps.propGetInt(kOfxParamPropDisplayMax, 0);
    ymax = _paramProps.propGetInt(kOfxParamPropDisplayMax, 1);
  }

  /** @brief get value */
  void Int2DParam::getValue(int &x, int &y)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValue(_paramHandle, &x, &y);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void Int2DParam::getValueAtTime(double t, int &x, int &y)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValueAtTime(_paramHandle, t, &x, &y);
    throwSuiteStatusException(stat);
  }

  /** @brief set value */
  void Int2DParam::setValue(int x, int y)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValue(_paramHandle, x, y);
    throwSuiteStatusException(stat);
  }

  /** @brief set the value at a time, implicitly adds a keyframe */
  void Int2DParam::setValueAtTime(double t, int x, int y)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramSetValueAtTime) throwHostMissingSuiteException("paramSetValueAtTime");
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValueAtTime(_paramHandle, t, x, y);
    throwSuiteStatusException(stat);
  }

  /** @brief delete all keys and set to default value */
  void Int2DParam::resetToDefault()
  {
    deleteAllKeys();
    int v0, v1;
    getDefault(v0, v1);
    setValue(v0, v1);
  }


  ////////////////////////////////////////////////////////////////////////////////
  // 3D Int params

  /** @brief hidden constructor */
  Int3DParam::Int3DParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : ValueParam(paramSet, name, eInt3DParam, handle)
  {
  }

  /** @brief set the default value */
  void Int3DParam::setDefault(int x, int y, int z)
  {
    _paramProps.propSetInt(kOfxParamPropDefault, x, 0);
    _paramProps.propSetInt(kOfxParamPropDefault, y, 1);
    _paramProps.propSetInt(kOfxParamPropDefault, z, 2);
  }

  /** @brief set the hard min/max range, default is INT_MIN, INT_MAX */
  void 
    Int3DParam::setRange(int xmin, int ymin, int zmin,
    int xmax, int ymax, int zmax)
  {
    _paramProps.propSetInt(kOfxParamPropMin, xmin, 0);
    _paramProps.propSetInt(kOfxParamPropMin, ymin, 1);
    _paramProps.propSetInt(kOfxParamPropMin, zmin, 2);
    _paramProps.propSetInt(kOfxParamPropMax, xmax, 0);
    _paramProps.propSetInt(kOfxParamPropMax, ymax, 1);
    _paramProps.propSetInt(kOfxParamPropMax, zmax, 2);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    Int3DParam::setDisplayRange(int xmin, int ymin, int zmin,
    int xmax, int ymax, int zmax)
  {
    _paramProps.propSetInt(kOfxParamPropDisplayMin, xmin, 0);
    _paramProps.propSetInt(kOfxParamPropDisplayMin, ymin, 1);
    _paramProps.propSetInt(kOfxParamPropDisplayMin, zmin, 2);
    _paramProps.propSetInt(kOfxParamPropDisplayMax, xmax, 0);
    _paramProps.propSetInt(kOfxParamPropDisplayMax, ymax, 1);
    _paramProps.propSetInt(kOfxParamPropDisplayMax, zmax, 2);
  }

  /** @brief get the default value */
  void Int3DParam::getDefault(int &x, int &y, int &z)
  {    
    x = _paramProps.propGetInt(kOfxParamPropDefault, 0);
    y = _paramProps.propGetInt(kOfxParamPropDefault, 1);
    z = _paramProps.propGetInt(kOfxParamPropDefault, 2);
  }


  /** @brief set the hard min/max range, default is INT_MIN, INT_MAX */
  void 
    Int3DParam::getRange(int &xmin, int &ymin, int &zmin,
    int &xmax, int &ymax, int &zmax)
  {
    xmin = _paramProps.propGetInt(kOfxParamPropMin, 0);
    ymin = _paramProps.propGetInt(kOfxParamPropMin, 1);
    zmin = _paramProps.propGetInt(kOfxParamPropMin, 2);
    xmax = _paramProps.propGetInt(kOfxParamPropMax, 0);
    ymax = _paramProps.propGetInt(kOfxParamPropMax, 1);
    zmax = _paramProps.propGetInt(kOfxParamPropMax, 2);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    Int3DParam::getDisplayRange(int &xmin, int &ymin, int &zmin,
    int &xmax, int &ymax, int &zmax)
  {
    xmin = _paramProps.propGetInt(kOfxParamPropDisplayMin, 0);
    ymin = _paramProps.propGetInt(kOfxParamPropDisplayMin, 1);
    zmin = _paramProps.propGetInt(kOfxParamPropDisplayMin, 2);
    xmax = _paramProps.propGetInt(kOfxParamPropDisplayMax, 0);
    ymax = _paramProps.propGetInt(kOfxParamPropDisplayMax, 1);
    zmax = _paramProps.propGetInt(kOfxParamPropDisplayMax, 2);
  }

  /** @brief get value */
  void Int3DParam::getValue(int &x, int &y, int &z)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValue(_paramHandle, &x, &y, &z);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void Int3DParam::getValueAtTime(double t, int &x, int &y, int &z)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValueAtTime(_paramHandle, t, &x, &y, &z);
    throwSuiteStatusException(stat);
  }

  /** @brief set value */
  void Int3DParam::setValue(int x, int y, int z)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValue(_paramHandle, x, y, z);
    throwSuiteStatusException(stat);
  }

  /** @brief set the value at a time, implicitly adds a keyframe */
  void Int3DParam::setValueAtTime(double t, int x, int y, int z)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramSetValueAtTime) throwHostMissingSuiteException("paramSetValueAtTime");
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValueAtTime(_paramHandle, t, x, y, z);
    throwSuiteStatusException(stat);
  }

  /** @brief delete all keys and set to default value */
  void Int3DParam::resetToDefault()
  {
    deleteAllKeys();
    int v0, v1, v2;
    getDefault(v0, v1, v2);
    setValue(v0, v1, v2);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // common base to all double params

  /** @brief hidden constructor */
  BaseDoubleParam::BaseDoubleParam(const ParamSet *paramSet, const std::string &name, ParamTypeEnum type, OfxParamHandle handle)
    : ValueParam(paramSet, name, type, handle)
  {
  }

  /** @brief set the sensitivity of any gui slider */
  void BaseDoubleParam::setIncrement(double v)
  {
    _paramProps.propSetDouble(kOfxParamPropIncrement, v);
  }

  /** @brief set the number of digits printed after a decimal point in any gui */
  void BaseDoubleParam::setDigits(int v)
  {
    _paramProps.propSetInt(kOfxParamPropDigits, v);
  }

  /** @brief get the sensitivity of any gui slider */
  void BaseDoubleParam::getIncrement(double &v)
  {
    v = _paramProps.propGetDouble(kOfxParamPropIncrement);
  }

  /** @brief get the number of digits printed after a decimal point in any gui */
  void BaseDoubleParam::getDigits(int &v)
  {
    v = _paramProps.propGetInt(kOfxParamPropDigits);
  }

  /** @brief get the type of the double param, defaults to eDoubleTypePlain */
  void BaseDoubleParam::getDoubleType(DoubleTypeEnum &v)
  {
    std::string str = _paramProps.propGetString(kOfxParamPropDoubleType);

    if(str == kOfxParamDoubleTypePlain)
      v = eDoubleTypePlain;
    else if(str == kOfxParamDoubleTypeAngle)
      v = eDoubleTypeAngle;
    else if(str == kOfxParamDoubleTypeScale)
      v = eDoubleTypeScale;
    else if(str == kOfxParamDoubleTypeTime)
      v = eDoubleTypeTime;
    else if(str == kOfxParamDoubleTypeAbsoluteTime)
      v = eDoubleTypeAbsoluteTime;
    else if(str == kOfxParamDoubleTypeX)
      v = eDoubleTypeX;
    else if(str == kOfxParamDoubleTypeXAbsolute)
      v = eDoubleTypeXAbsolute;
    else if(str == kOfxParamDoubleTypeY)
      v = eDoubleTypeY;
    else if(str == kOfxParamDoubleTypeYAbsolute)
      v = eDoubleTypeYAbsolute;
    else if(str == kOfxParamDoubleTypeXY)
      v = eDoubleTypeXY;
    else if(str == kOfxParamDoubleTypeXYAbsolute)
      v = eDoubleTypeXYAbsolute;
#ifdef kOfxParamDoubleTypeNormalisedX
    else if(str == kOfxParamDoubleTypeNormalisedX)
      v = eDoubleTypeNormalisedX;
    else if(str == kOfxParamDoubleTypeNormalisedY)
      v = eDoubleTypeNormalisedY;
    else if(str == kOfxParamDoubleTypeNormalisedXAbsolute)
      v = eDoubleTypeNormalisedXAbsolute;
    else if(str == kOfxParamDoubleTypeNormalisedYAbsolute)
      v = eDoubleTypeNormalisedYAbsolute;
    else if(str == kOfxParamDoubleTypeNormalisedXY)
      v = eDoubleTypeNormalisedXY;
    else if(str == kOfxParamDoubleTypeNormalisedXYAbsolute)
      v = eDoubleTypeNormalisedXYAbsolute;
#endif
    else
      v = eDoubleTypePlain;
  }

  /** @brief get the type of coordinate system for default values */
  void BaseDoubleParam::getDefaultCoordinateSystem(DefaultCoordinateSystemEnum &v)
  {
    std::string str = _paramProps.propGetString(kOfxParamPropDefaultCoordinateSystem);

    if(str == kOfxParamCoordinatesNormalised)
      v = eCoordinatesNormalised;
    else
      v = eCoordinatesCanonical;
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Wraps up an double param */

  /** @brief hidden constructor */
  DoubleParam::DoubleParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : BaseDoubleParam(paramSet, name, eDoubleParam, handle)
  {
  }

  /** @brief set the default value */
  void DoubleParam::setDefault(double v)
  {
    _paramProps.propSetDouble(kOfxParamPropDefault, v);
  }

  /** @brief set the hard min/max range, default is DOUBLE_MIN, DOUBLE_MAX */
  void DoubleParam::setRange(double min, double max)
  {
    _paramProps.propSetDouble(kOfxParamPropMin, min);
    _paramProps.propSetDouble(kOfxParamPropMax, max);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void DoubleParam::setDisplayRange(double min, double max)
  {
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, min);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, max);
  }

  /** @brief get the default value */
  void DoubleParam::getDefault(double &v)
  {    
    v = _paramProps.propGetDouble(kOfxParamPropDefault);
  }

  /** @brief set the hard min/max range, default is DOUBLE_MIN, DOUBLE_MAX */
  void DoubleParam::getRange(double &min, double &max)
  {
    min = _paramProps.propGetDouble(kOfxParamPropMin);
    max = _paramProps.propGetDouble(kOfxParamPropMax);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void DoubleParam::getDisplayRange(double &min, double &max)
  {
    min = _paramProps.propGetDouble(kOfxParamPropDisplayMin);
    max = _paramProps.propGetDouble(kOfxParamPropDisplayMax);
  }

  /** @brief get value */
  void DoubleParam::getValue(double &v)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValue(_paramHandle, &v);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void DoubleParam::getValueAtTime(double t, double &v)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValueAtTime(_paramHandle, t, &v);
    throwSuiteStatusException(stat);
  }

  /** @brief set value */
  void DoubleParam::setValue(double v)
  {
    if (v != v) {
      // value is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValue(_paramHandle, v);
    throwSuiteStatusException(stat);
  }

  /** @brief set the value at a time, implicitly adds a keyframe */
  void DoubleParam::setValueAtTime(double t, double v)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if (v != v) {
      // value is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramSetValueAtTime) throwHostMissingSuiteException("paramSetValueAtTime");
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValueAtTime(_paramHandle, t, v);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void DoubleParam::differentiate(double t, double &v)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramGetDerivative) throwHostMissingSuiteException("paramGetDerivative");
    OfxStatus stat = OFX::Private::gParamSuite->paramGetDerivative(_paramHandle, t, &v);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void DoubleParam::integrate(double t1, double t2, double &v)
  {
    if (t1 != t1 || t2 != t2) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramGetIntegral) throwHostMissingSuiteException("paramGetIntegral");
    OfxStatus stat = OFX::Private::gParamSuite->paramGetIntegral(_paramHandle, t1, t2, &v);
    throwSuiteStatusException(stat);
  }

  /** @brief delete all keys and set to default value */
  void DoubleParam::resetToDefault()
  {
    deleteAllKeys();
    double v;
    getDefault(v);
    setValue(v);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // 2D Double params

  /** @brief hidden constructor */
  Double2DParam::Double2DParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : BaseDoubleParam(paramSet, name, eDouble2DParam, handle)
  {
  }

  /** @brief set the default value */
  void Double2DParam::setDefault(double x, double y)
  {
    _paramProps.propSetDouble(kOfxParamPropDefault, x, 0);
    _paramProps.propSetDouble(kOfxParamPropDefault, y, 1);
  }

  /** @brief set the hard min/max range, default is DOUBLE_MIN, DOUBLE_MAX */
  void 
    Double2DParam::setRange(double xmin, double ymin,
    double xmax, double ymax)
  {
    _paramProps.propSetDouble(kOfxParamPropMin, xmin, 0);
    _paramProps.propSetDouble(kOfxParamPropMin, ymin, 1);
    _paramProps.propSetDouble(kOfxParamPropMax, xmax, 0);
    _paramProps.propSetDouble(kOfxParamPropMax, ymax, 1);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    Double2DParam::setDisplayRange(double xmin, double ymin,
    double xmax, double ymax)
  {
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, xmin, 0);
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, ymin, 1);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, xmax, 0);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, ymax, 1);
  }

  /** @brief get the default value */
  void Double2DParam::getDefault(double &x, double &y)
  {    
    x = _paramProps.propGetDouble(kOfxParamPropDefault, 0);
    y = _paramProps.propGetDouble(kOfxParamPropDefault, 1);
  }


  /** @brief set the hard min/max range, default is DOUBLE_MIN, DOUBLE_MAX */
  void 
    Double2DParam::getRange(double &xmin, double &ymin,
    double &xmax, double &ymax)
  {
    xmin = _paramProps.propGetDouble(kOfxParamPropMin, 0);
    ymin = _paramProps.propGetDouble(kOfxParamPropMin, 1);
    xmax = _paramProps.propGetDouble(kOfxParamPropMax, 0);
    ymax = _paramProps.propGetDouble(kOfxParamPropMax, 1);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    Double2DParam::getDisplayRange(double &xmin, double &ymin,
    double &xmax, double &ymax)
  {
    xmin = _paramProps.propGetDouble(kOfxParamPropDisplayMin, 0);
    ymin = _paramProps.propGetDouble(kOfxParamPropDisplayMin, 1);
    xmax = _paramProps.propGetDouble(kOfxParamPropDisplayMax, 0);
    ymax = _paramProps.propGetDouble(kOfxParamPropDisplayMax, 1);
  }

  /** @brief get value */
  void Double2DParam::getValue(double &x, double &y)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValue(_paramHandle, &x, &y);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void Double2DParam::getValueAtTime(double t, double &x, double &y)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValueAtTime(_paramHandle, t, &x, &y);
    throwSuiteStatusException(stat);
  }

  /** @brief set value */
  void Double2DParam::setValue(double x, double y)
  {
    if (x != x || y != y) {
      // value is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValue(_paramHandle, x, y);
    throwSuiteStatusException(stat);
  }

  /** @brief set the value at a time, implicitly adds a keyframe */
  void Double2DParam::setValueAtTime(double t, double x, double y)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if (x != x || y != y) {
      // value is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramSetValueAtTime) throwHostMissingSuiteException("paramSetValueAtTime");
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValueAtTime(_paramHandle, t, x, y);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void Double2DParam::differentiate(double t, double &x, double &y)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramGetDerivative) throwHostMissingSuiteException("paramGetDerivative");
    OfxStatus stat = OFX::Private::gParamSuite->paramGetDerivative(_paramHandle, t, &x, &y);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void Double2DParam::integrate(double t1, double t2, double &x, double &y)
  {
    if (t1 != t1 || t2 != t2) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramGetIntegral) throwHostMissingSuiteException("paramGetIntegral");
    OfxStatus stat = OFX::Private::gParamSuite->paramGetIntegral(_paramHandle, t1, t2, &x, &y);
    throwSuiteStatusException(stat);
  }

  /** @brief delete all keys and set to default value */
  void Double2DParam::resetToDefault()
  {
    deleteAllKeys();
    double v0, v1;
    getDefault(v0, v1);
    setValue(v0, v1);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // 3D Double params

  /** @brief hidden constructor */
  Double3DParam::Double3DParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : BaseDoubleParam(paramSet, name, eDouble3DParam, handle)
  {
  }

  /** @brief set the default value */
  void Double3DParam::setDefault(double x, double y, double z)
  {
    _paramProps.propSetDouble(kOfxParamPropDefault, x, 0);
    _paramProps.propSetDouble(kOfxParamPropDefault, y, 1);
    _paramProps.propSetDouble(kOfxParamPropDefault, z, 2);
  }

  /** @brief set the hard min/max range, default is DOUBLE_MIN, DOUBLE_MAX */
  void 
    Double3DParam::setRange(double xmin, double ymin, double zmin,
    double xmax, double ymax, double zmax)
  {
    _paramProps.propSetDouble(kOfxParamPropMin, xmin, 0);
    _paramProps.propSetDouble(kOfxParamPropMin, ymin, 1);
    _paramProps.propSetDouble(kOfxParamPropMin, zmin, 2);
    _paramProps.propSetDouble(kOfxParamPropMax, xmax, 0);
    _paramProps.propSetDouble(kOfxParamPropMax, ymax, 1);
    _paramProps.propSetDouble(kOfxParamPropMax, zmax, 2);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    Double3DParam::setDisplayRange(double xmin, double ymin, double zmin,
    double xmax, double ymax, double zmax)
  {
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, xmin, 0);
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, ymin, 1);
    _paramProps.propSetDouble(kOfxParamPropDisplayMin, zmin, 2);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, xmax, 0);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, ymax, 1);
    _paramProps.propSetDouble(kOfxParamPropDisplayMax, zmax, 2);
  }

  /** @brief get the default value */
  void Double3DParam::getDefault(double &x, double &y, double &z)
  {    
    x = _paramProps.propGetDouble(kOfxParamPropDefault, 0);
    y = _paramProps.propGetDouble(kOfxParamPropDefault, 1);
    z = _paramProps.propGetDouble(kOfxParamPropDefault, 2);
  }


  /** @brief set the hard min/max range, default is DOUBLE_MIN, DOUBLE_MAX */
  void 
    Double3DParam::getRange(double &xmin, double &ymin, double &zmin,
    double &xmax, double &ymax, double &zmax)
  {
    xmin = _paramProps.propGetDouble(kOfxParamPropMin, 0);
    ymin = _paramProps.propGetDouble(kOfxParamPropMin, 1);
    zmin = _paramProps.propGetDouble(kOfxParamPropMin, 2);
    xmax = _paramProps.propGetDouble(kOfxParamPropMax, 0);
    ymax = _paramProps.propGetDouble(kOfxParamPropMax, 1);
    zmax = _paramProps.propGetDouble(kOfxParamPropMax, 2);
  }

  /** @brief set the display min and max, default is to be the same as the range param */
  void 
    Double3DParam::getDisplayRange(double &xmin, double &ymin, double &zmin,
    double &xmax, double &ymax, double &zmax)
  {
    xmin = _paramProps.propGetDouble(kOfxParamPropDisplayMin, 0);
    ymin = _paramProps.propGetDouble(kOfxParamPropDisplayMin, 1);
    zmin = _paramProps.propGetDouble(kOfxParamPropDisplayMin, 2);
    xmax = _paramProps.propGetDouble(kOfxParamPropDisplayMax, 0);
    ymax = _paramProps.propGetDouble(kOfxParamPropDisplayMax, 1);
    zmax = _paramProps.propGetDouble(kOfxParamPropDisplayMax, 2);
  }

  /** @brief get value */
  void Double3DParam::getValue(double &x, double &y, double &z)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValue(_paramHandle, &x, &y, &z);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void Double3DParam::getValueAtTime(double t, double &x, double &y, double &z)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValueAtTime(_paramHandle, t, &x, &y, &z);
    throwSuiteStatusException(stat);
  }

  /** @brief set value */
  void Double3DParam::setValue(double x, double y, double z)
  {
    if (x != x || y != y || z != z) {
      // value is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValue(_paramHandle, x, y, z);
    throwSuiteStatusException(stat);
  }

  /** @brief set the value at a time, implicitly adds a keyframe */
  void Double3DParam::setValueAtTime(double t, double x, double y, double z)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if (x != x || y != y || z != z) {
      // value is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramSetValueAtTime) throwHostMissingSuiteException("paramSetValueAtTime");
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValueAtTime(_paramHandle, t, x, y, z);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void Double3DParam::differentiate(double t, double &x, double &y, double &z)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramGetDerivative) throwHostMissingSuiteException("paramGetDerivative");
    OfxStatus stat = OFX::Private::gParamSuite->paramGetDerivative(_paramHandle, t, &x, &y, &z);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void Double3DParam::integrate(double t1, double t2, double &x, double &y, double &z)
  {
    if (t1 != t1 || t2 != t2) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramGetIntegral) throwHostMissingSuiteException("paramGetIntegral");
    OfxStatus stat = OFX::Private::gParamSuite->paramGetIntegral(_paramHandle, t1, t2, &x, &y, &z);
    throwSuiteStatusException(stat);
  }

  /** @brief delete all keys and set to default value */
  void Double3DParam::resetToDefault()
  {
    deleteAllKeys();
    double v0, v1, v2;
    getDefault(v0, v1, v2);
    setValue(v0, v1, v2);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // RGB colour param
  /** @brief hidden constructor */
  RGBParam::RGBParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : ValueParam(paramSet, name, eRGBParam, handle)
  {
  }

  /** @brief set the default value */
  void RGBParam::setDefault(double r, double g, double b)
  {
    _paramProps.propSetDouble(kOfxParamPropDefault, r, 0);
    _paramProps.propSetDouble(kOfxParamPropDefault, g, 1);
    _paramProps.propSetDouble(kOfxParamPropDefault, b, 2);
  }


  /** @brief get the default value */
  void RGBParam::getDefault(double &r, double &g, double &b)
  {    
    r = _paramProps.propGetDouble(kOfxParamPropDefault, 0);
    g = _paramProps.propGetDouble(kOfxParamPropDefault, 1);
    b = _paramProps.propGetDouble(kOfxParamPropDefault, 2);
  }

  /** @brief get value */
  void RGBParam::getValue(double &r, double &g, double &b)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValue(_paramHandle, &r, &g, &b);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void RGBParam::getValueAtTime(double t, double &r, double &g, double &b)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValueAtTime(_paramHandle, t, &r, &g, &b);
    throwSuiteStatusException(stat);
  }

  /** @brief set value */
  void RGBParam::setValue(double r, double g, double b)
  {
    if (r != r || g != g || b != b) {
      // value is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValue(_paramHandle, r, g, b);
    throwSuiteStatusException(stat);
  }

  /** @brief set the value at a time, implicitly adds a keyframe */
  void RGBParam::setValueAtTime(double t, double r, double g, double b)
  {
    if (r != r || g != g || b != b) {
      // value is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramSetValueAtTime) throwHostMissingSuiteException("paramSetValueAtTime");
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValueAtTime(_paramHandle, t, r, g, b);
    throwSuiteStatusException(stat);
  }

  /** @brief delete all keys and set to default value */
  void RGBParam::resetToDefault()
  {
    deleteAllKeys();
    double v0, v1, v2;
    getDefault(v0, v1, v2);
    setValue(v0, v1, v2);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // RGBA colour param
  /** @brief hidden constructor */
  RGBAParam::RGBAParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : ValueParam(paramSet, name, eRGBAParam, handle)
  {
  }

  /** @brief set the default value */
  void RGBAParam::setDefault(double r, double g, double b, double a)
  {
    _paramProps.propSetDouble(kOfxParamPropDefault, r, 0);
    _paramProps.propSetDouble(kOfxParamPropDefault, g, 1);
    _paramProps.propSetDouble(kOfxParamPropDefault, b, 2);
    _paramProps.propSetDouble(kOfxParamPropDefault, a, 3);
  }


  /** @brief get the default value */
  void RGBAParam::getDefault(double &r, double &g, double &b, double &a)
  {    
    r = _paramProps.propGetDouble(kOfxParamPropDefault, 0);
    g = _paramProps.propGetDouble(kOfxParamPropDefault, 1);
    b = _paramProps.propGetDouble(kOfxParamPropDefault, 2);
    a = _paramProps.propGetDouble(kOfxParamPropDefault, 3);
  }

  /** @brief get value */
  void RGBAParam::getValue(double &r, double &g, double &b, double &a)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValue(_paramHandle, &r, &g, &b, &a);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void RGBAParam::getValueAtTime(double t, double &r, double &g, double &b, double &a)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValueAtTime(_paramHandle, t, &r, &g, &b, &a);
    throwSuiteStatusException(stat);
  }

  /** @brief set value */
  void RGBAParam::setValue(double r, double g, double b, double a)
  {
    if (r != r || g != g || b != b || a != a) {
      // value is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValue(_paramHandle, r, g, b, a);
    throwSuiteStatusException(stat);
  }

  /** @brief set the value at a time, implicitly adds a keyframe */
  void RGBAParam::setValueAtTime(double t, double r, double g, double b, double a)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if (r != r || g != g || b != b || a != a) {
      // value is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramSetValueAtTime) throwHostMissingSuiteException("paramSetValueAtTime");
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValueAtTime(_paramHandle, t, r, g, b, a);
    throwSuiteStatusException(stat);
  }

  /** @brief delete all keys and set to default value */
  void RGBAParam::resetToDefault()
  {
    deleteAllKeys();
    double v0, v1, v2, v3;
    getDefault(v0, v1, v2, v3);
    setValue(v0, v1, v2, v3);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Wraps up a string param */

  /** @brief hidden constructor */
  StringParam::StringParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : ValueParam(paramSet, name, eStringParam, handle)
  {
  }

  /** @brief set the default value */
  void StringParam::setDefault(const std::string &v)
  {
    _paramProps.propSetString(kOfxParamPropDefault, v);
  }

  /** @brief get the default value */
  void StringParam::getDefault(std::string &v)
  {    
    v = _paramProps.propGetString(kOfxParamPropDefault);
  }

  /** @brief get value */
  void StringParam::getValue(std::string &v)
  {
    char *cStr;
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValue(_paramHandle, &cStr);
    throwSuiteStatusException(stat);
# ifdef DEBUG
    if (!is_utf8(cStr)) {
      std::cout << "Warning: string param '" << getName() << "' contains a non-UTF-8 string: " << cStr << std::endl;
    }
# endif
    v = cStr;
  }

  /** @brief get the value at a time */
  void StringParam::getValueAtTime(double t, std::string &v)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    char *cStr;
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValueAtTime(_paramHandle, t, &cStr);
    throwSuiteStatusException(stat);
# ifdef DEBUG
    if (!is_utf8(cStr)) {
      std::cout << "Warning: string param '" << getName() << "' contains a non-UTF-8 string: " << cStr << std::endl;
    }
# endif
    v = cStr;
  }

  /** @brief set value */
  void StringParam::setValue(const std::string &v)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValue(_paramHandle, v.c_str());
    throwSuiteStatusException(stat);
# ifdef DEBUG
    if (!is_utf8(v.c_str())) {
      std::cout << "Warning: string param '" << getName() << "' contains a non-UTF-8 string: " << v.c_str() << std::endl;
    }
# endif
  }

  /** @brief set the value at a time, implicitly adds a keyframe */
  void StringParam::setValueAtTime(double t, const std::string &v)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramSetValueAtTime) throwHostMissingSuiteException("paramSetValueAtTime");
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValueAtTime(_paramHandle, t, v.c_str());
    throwSuiteStatusException(stat);
# ifdef DEBUG
    if (!is_utf8(v.c_str())) {
      std::cout << "Warning: string param '" << getName() << "' contains a non-UTF-8 string: " << v.c_str() << std::endl;
    }
# endif
  }

  /** @brief delete all keys and set to default value */
  void StringParam::resetToDefault()
  {
    deleteAllKeys();
    std::string v;
    getDefault(v);
    setValue(v);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Wraps up a Boolean integer param */

  /** @brief hidden constructor */
  BooleanParam::BooleanParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : ValueParam(paramSet, name, eBooleanParam, handle)
  {
  }

  /** @brief set the default value */
  void BooleanParam::setDefault(bool v)
  {
    _paramProps.propSetInt(kOfxParamPropDefault, v);
  }

  /** @brief get the default value */
  void BooleanParam::getDefault(bool &v)
  {    
    v = _paramProps.propGetInt(kOfxParamPropDefault) != 0;
  }

  /** @brief get value */
  void BooleanParam::getValue(bool &v)
  {
    int iVal;
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValue(_paramHandle, &iVal);
    throwSuiteStatusException(stat);
    v = iVal != 0;
  }

  /** @brief get the value at a time */
  void BooleanParam::getValueAtTime(double t, bool &v)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    int iVal;
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValueAtTime(_paramHandle, t, &iVal);
    throwSuiteStatusException(stat);
    v = iVal != 0;
  }

  /** @brief set value */
  void BooleanParam::setValue(bool v)
  {
    int iVal = v;
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValue(_paramHandle, iVal);
    throwSuiteStatusException(stat);
  }

  /** @brief set the value at a time, implicitly adds a keyframe */
  void BooleanParam::setValueAtTime(double t, bool v)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramSetValueAtTime) throwHostMissingSuiteException("paramSetValueAtTime");
    int iVal = v;
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValueAtTime(_paramHandle, t, iVal);
    throwSuiteStatusException(stat);
  }

  /** @brief delete all keys and set to default value */
  void BooleanParam::resetToDefault()
  {
    deleteAllKeys();
    bool v;
    getDefault(v);
    setValue(v);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Wraps up a choice integer param */

  /** @brief hidden constructor */
  ChoiceParam::ChoiceParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : ValueParam(paramSet, name, eChoiceParam, handle)
  {
  }

  /** @brief set the default value */
  void ChoiceParam::setDefault(int v)
  {
    _paramProps.propSetInt(kOfxParamPropDefault, v);
  }

  /** @brief get the default value */
  void ChoiceParam::getDefault(int &v)
  {    
    v = _paramProps.propGetInt(kOfxParamPropDefault);
  }

  /** @brief get value */
  void ChoiceParam::getValue(int &v)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValue(_paramHandle, &v);
    throwSuiteStatusException(stat);
  }

  /** @brief get the value at a time */
  void ChoiceParam::getValueAtTime(double t, int &v)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValueAtTime(_paramHandle, t, &v);
    throwSuiteStatusException(stat);
  }

  /** @brief set value */
  void ChoiceParam::setValue(int v)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValue(_paramHandle, v);
    throwSuiteStatusException(stat);
  }

  /** @brief set the value at a time, implicitly adds a keyframe */
  void ChoiceParam::setValueAtTime(double t, int v)
  {
    if(!OFX::Private::gParamSuite->paramSetValueAtTime) throwHostMissingSuiteException("paramSetValueAtTime");
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValueAtTime(_paramHandle, t, v);
    throwSuiteStatusException(stat);
  }

  /** @brief how many options do we have */
  int ChoiceParam::getNOptions(void)
  {
    int nCurrentValues = _paramProps.propGetDimension(kOfxParamPropChoiceOption);
    return nCurrentValues;
  }

  /** @brief get the option value */
  void ChoiceParam::getOption(int ix, std::string &v)
  {    
    v = _paramProps.propGetString(kOfxParamPropChoiceOption, ix);
  }
  
  void ChoiceParam::getOptions(std::vector<std::string>* options, std::vector<std::string>* labels)
  {
    _paramProps.propGetNString(kOfxParamPropChoiceOption, options);
#ifdef OFX_EXTENSIONS_TUTTLE
    if (labels) {
      _paramProps.propGetNString(kOfxParamPropChoiceLabelOption, labels, false);
    }
#endif
  }

  /** @brief add another option */
  void ChoiceParam::appendOption(const std::string &v, const std::string& label)
  {
    int nCurrentValues = _paramProps.propGetDimension(kOfxParamPropChoiceOption);
    if(!label.empty()) {
#ifdef OFX_EXTENSIONS_TUTTLE
      // Choice label is an ofx extension. If the host doesn't support it,
      // we put this information into the parameter hint.
      // from https://github.com/tuttleofx/TuttleOFX/commit/ae6e14e99f62b5efa89e4de4a3bc33129ac6afd0
      try {
        // this property is an optional extension.
         _paramProps.propSetString(kOfxParamPropChoiceLabelOption, label, nCurrentValues);
      } catch(std::exception&)
#endif
      {
        // If the kOfxParamPropChoiceLabelOption doesn't exist, we put that information into the Hint.
        // It's better than nothing...
        std::string hint = _paramProps.propGetString(kOfxParamPropHint);
        if(!hint.empty()) {
          hint += "\n";
          if( nCurrentValues == 0 ) {
            hint += "\n";
          }
        }
        hint += v + ": " + label;
        _paramProps.propSetString(kOfxParamPropHint, hint);
      }
    }
    _paramProps.propSetString(kOfxParamPropChoiceOption, v, nCurrentValues);
  }

  /** @brief set the string of a specific option */
  void ChoiceParam::setOption(int item, const std::string &str)
  {
    _paramProps.propSetString(kOfxParamPropChoiceOption, str, item);
  }

  /** @brief set to the default value */
  void ChoiceParam::resetOptions(const std::vector<std::string>& newEntries,
                                 const std::vector<std::string>& newEntriesLabel)
  {
    assert(newEntries.size() == newEntriesLabel.size() || newEntriesLabel.empty());
    if (newEntries.empty() || (newEntries.size() != newEntriesLabel.size() && !newEntriesLabel.empty())) {
      
      // Invalid parameters or empty newEntries, reset the property
#ifdef OFX_EXTENSIONS_TUTTLE
      if (_paramProps.propGetDimension(kOfxParamPropChoiceLabelOption, false) > 0) {
        _paramProps.propReset(kOfxParamPropChoiceLabelOption);
      }
#endif
      _paramProps.propReset(kOfxParamPropChoiceOption);
    } else {
      // Set the new entries
#ifdef OFX_EXTENSIONS_TUTTLE
      if (!newEntriesLabel.empty()) {
        _paramProps.propSetStringN(kOfxParamPropChoiceLabelOption, newEntriesLabel, false);
      }
#endif
      _paramProps.propSetStringN(kOfxParamPropChoiceOption, newEntries);
    }

  }

  /** @brief delete all keys and set to default value */
  void ChoiceParam::resetToDefault()
  {
    deleteAllKeys();
    int v;
    getDefault(v);
    setValue(v);
  }

#ifdef OFX_EXTENSIONS_NATRON
    /** @brief whether the menu should be cascading, and each option contains a slash-separated path to the item, defaults to false. */
    bool ChoiceParam::getIsCascading()
    {
        bool v = _paramProps.propGetInt(kNatronOfxParamPropChoiceCascading, false) != 0;
        return v;
    }

    /** @brief Indicate whether the host can add a new choice on its own (probably via a GUI specific to this parameter).
     The plugin may then retrieve the option name whenever a choice value is out of its initial range.

     This property primarily targets image plane choices, where the host should be able to create a new plane and add it to the menu.
     */
    bool ChoiceParam::getHostCanAddOptions()
    {
        bool v = _paramProps.propGetInt(kNatronOfxParamPropChoiceHostCanAddOptions, false) != 0;
        return v;
    }
#endif

  ////////////////////////////////////////////////////////////////////////////////
  // Wraps up a custom param */

  /** @brief hidden constructor */
  CustomParam::CustomParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : ValueParam(paramSet, name, eCustomParam, handle)
  {
  }

  /** @brief set the default value */
  void CustomParam::setDefault(const std::string &v)
  {
    _paramProps.propSetString(kOfxParamPropDefault, v);
  }

  /** @brief get the default value */
  void CustomParam::getDefault(std::string &v)
  {    
    v = _paramProps.propGetString(kOfxParamPropDefault);
  }

  /** @brief get value */
  void CustomParam::getValue(std::string &v)
  {
    char *cStr;
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValue(_paramHandle, &cStr);
    throwSuiteStatusException(stat);
    v = cStr;
  }

  /** @brief get the value at a time */
  void CustomParam::getValueAtTime(double t, std::string &v)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    char *cStr;
    OfxStatus stat = OFX::Private::gParamSuite->paramGetValueAtTime(_paramHandle, t, &cStr);
    throwSuiteStatusException(stat);
    v = cStr;
  }

  /** @brief set value */
  void CustomParam::setValue(const std::string &v)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValue(_paramHandle, v.c_str());
    throwSuiteStatusException(stat);
  }

  /** @brief set value */
  void CustomParam::setValue(const char* str)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValue(_paramHandle, str);
    throwSuiteStatusException(stat);
  }

  /** @brief set the value at a time, implicitly adds a keyframe */
  void CustomParam::setValueAtTime(double t, const std::string &v)
  {
    if (t != t) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    if(!OFX::Private::gParamSuite->paramSetValueAtTime) throwHostMissingSuiteException("paramSetValueAtTime");
    OfxStatus stat = OFX::Private::gParamSuite->paramSetValueAtTime(_paramHandle, t, v.c_str());
    throwSuiteStatusException(stat);
  }

  /** @brief delete all keys and set to default value */
  void CustomParam::resetToDefault()
  {
    deleteAllKeys();
    std::string v;
    getDefault(v);
    setValue(v);
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Wraps up a group param
  /** @brief hidden constructor */
  GroupParam::GroupParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : Param(paramSet, name, eGroupParam, handle)
  {
  }

  /** @brief whether the group is open or closed in a hierarchical layout */
  bool GroupParam::getIsOpen()
  {
    bool v = _paramProps.propGetInt(kOfxParamPropGroupOpen, false) != 0; // introduced in OFX 1.2
    return v;
  }

  /** @brief whether the group is open or closed in a hierarchical layout, defaults to true */
  void GroupParam::setOpen(const bool v)
  {
    _paramProps.propSetInt(kOfxParamPropGroupOpen, v, false); // introduced in OFX 1.2
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Wraps up a page param

  /** @brief hidden constructor */
  PageParam::PageParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : Param(paramSet, name, ePageParam, handle)
  {
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Wraps up a PushButton param

  /** @brief hidden constructor */
  PushButtonParam::PushButtonParam(const ParamSet *paramSet, const std::string &name, OfxParamHandle handle)
    : Param(paramSet, name, ePushButtonParam, handle)
  {
  }

  ////////////////////////////////////////////////////////////////////////////////
  // Wraps up a Parametric param

  /** @brief hidden constructor */
  ParametricParam::ParametricParam(const ParamSet* paramSet, const std::string &name, OfxParamHandle handle)
      : Param(paramSet, name, eParametricParam, handle)
  {}

  /** @brief Evaluates a parametric parameter

      \arg curveIndex            which dimension to evaluate
      \arg time                  the time to evaluate to the parametric param at
      \arg parametricPosition    the position to evaluate the parametric param at

      @returns the double value is returned
  */
  double ParametricParam::getValue(const int curveIndex,
                                    const OfxTime time,
                                    const double parametricPosition)
  {
    if (time != time) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    double returnValue = 0.0;
    OfxStatus stat = OFX::Private::gParametricParameterSuite->parametricParamGetValue(_paramHandle,
                                                                                       curveIndex,
                                                                                       time,
                                                                                       parametricPosition,
                                                                                       &returnValue);
    throwSuiteStatusException(stat);
    return returnValue;
  }

  /** @brief Returns the number of control points in the parametric param.

      \arg curveIndex            which dimension to check
      \arg time                  the time to check

      @returns the integer value is returned
  */
  int ParametricParam::getNControlPoints(const int curveIndex,
                                          const OfxTime time)
  {
    if (time != time) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    int returnValue = 0;
    OfxStatus stat = OFX::Private::gParametricParameterSuite->parametricParamGetNControlPoints(_paramHandle,
                                                                                                curveIndex,
                                                                                                time,
                                                                                                &returnValue);
    throwSuiteStatusException(stat);
    return returnValue;
  }

  /** @brief Returns the key/value pair of the nth control point.

      \arg curveIndex            which dimension to check
      \arg time                  the time to check
      \arg nthCtl                the nth control point to get the value of

      @returns a pair with key and value
  */
  std::pair<double, double> ParametricParam::getNthControlPoint(const int curveIndex,
                                                                const OfxTime time,
                                                                const int nthCtl)
  {
    if (time != time) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    std::pair<double, double> returnValue;
    OfxStatus stat = OFX::Private::gParametricParameterSuite->parametricParamGetNthControlPoint(_paramHandle,
                                                                                                 curveIndex,
                                                                                                 time,
                                                                                                 nthCtl,
                                                                                                 &returnValue.first,
                                                                                                 &returnValue.second);
    throwSuiteStatusException(stat);
    return returnValue;
  }

  /** @brief Modifies an existing control point on a curve

      \arg curveIndex            which dimension to set
      \arg time                  the time to set the value at
      \arg nthCtl                the control point to modify
      \arg key                   key of the control point
      \arg value                 value of the control point
      \arg addAnimationKey       if the param is an animatable, setting this to true will
      force an animation keyframe to be set as well as a curve key,
      otherwise if false, a key will only be added if the curve is already
      animating.

      @returns
      - ::kOfxStatOK            - all was fine
      - ::kOfxStatErrBadHandle  - if the paramter handle was invalid
      - ::kOfxStatErrUnknown    - if the type is unknown

      This modifies an existing control point. Note that by changing key, the order of the
      control point may be modified (as you may move it before or after anther point). So be
      careful when iterating over a curves control points and you change a key.
  */
  void ParametricParam::setNthControlPoints(const int curveIndex,
                                             const OfxTime time,
                                             const int nthCtl,
                                             const double key,
                                             const double value,
                                             const bool addAnimationKey)
  {
    if (time != time) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParametricParameterSuite->parametricParamSetNthControlPoint(_paramHandle,
                                                                                                 curveIndex,
                                                                                                 time,
                                                                                                 nthCtl,
                                                                                                 key,
                                                                                                 value,
                                                                                                 addAnimationKey);
    throwSuiteStatusException(stat);
  }

  void ParametricParam::setNthControlPoints(const int curveIndex,
                                             const OfxTime time,
                                             const int nthCtl,
                                             const std::pair<double, double> ctrlPoint,
                                             const bool addAnimationKey)
  {
    if (time != time) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    setNthControlPoints(curveIndex,
                         time,
                         nthCtl,
                         ctrlPoint.first,
                         ctrlPoint.second,
                         addAnimationKey);
  }

  /** @brief Adds a control point to the curve.

      \arg curveIndex            which dimension to set
      \arg time                  the time to set the value at
      \arg key                   key of the control point
      \arg value                 value of the control point
      \arg addAnimationKey       if the param is an animatable, setting this to true will
      force an animation keyframe to be set as well as a curve key,
      otherwise if false, a key will only be added if the curve is already
      animating.

      This will add a new control point to the given dimension of a parametric parameter. If a key exists
      sufficiently close to 'key', then it will be set to the indicated control point.
  */
  void ParametricParam::addControlPoint(const int curveIndex,
                                         const OfxTime time,
                                         const double key,
                                         const double value,
                                         const bool addAnimationKey)
  {
    if (time != time) {
      // time is NaN
      throwSuiteStatusException(kOfxStatErrValue);
    }
    OfxStatus stat = OFX::Private::gParametricParameterSuite->parametricParamAddControlPoint(_paramHandle, curveIndex, time, key, value, addAnimationKey);
    throwSuiteStatusException(stat);
  }

  /** @brief Deletes the nth control point from a parametric param.

      \arg curveIndex            which dimension to delete
      \arg nthCtl                the control point to delete
  */
  void ParametricParam::deleteControlPoint(const int curveIndex,
                                            const int nthCtl)
  {
    OfxStatus stat = OFX::Private::gParametricParameterSuite->parametricParamDeleteControlPoint(_paramHandle, curveIndex, nthCtl);
    throwSuiteStatusException(stat);
  }

  /** @brief Delete all curve control points on the given param.

      \arg curveIndex            which dimension to clear
  */
  void ParametricParam::deleteControlPoint(const int curveIndex)
  {
    OfxStatus stat = OFX::Private::gParametricParameterSuite->parametricParamDeleteAllControlPoints(_paramHandle, curveIndex);
    throwSuiteStatusException(stat);
  }

#ifdef OFX_EXTENSIONS_NUKE
  ////////////////////////////////////////////////////////////////////////////////
  // Wraps up a camera param

  /** @brief hidden constructor */
  CameraParam::CameraParam(/*OfxImageEffectHandle imageEffectHandle, */const ParamSet* paramSet, const std::string &name, NukeOfxCameraHandle handle)
      : Param(paramSet, name, eCameraParam, (OfxParamHandle)handle)
      //, _imageEffectHandle(imageEffectHandle)
  {
    // fetch all parameters
    // NukeOfxCameraHandle *camera;
    // OfxPropertySetHandle *propertySet;
    // OfxStatus stat = OFX::Private::gCameraParameterSuite->cameraGetHandle(_paramHandle, name.c_str(), camera, propertySet);
    // throwSuiteStatusException(stat);
  }

  Param* CameraParam::getParameter(const std::string &/*name*/)
  {
    return this;
  }
#endif

  ////////////////////////////////////////////////////////////////////////////////
  //  for a set of parameters
  /** @brief hidden ctor */
  ParamSet::ParamSet(void)
    : _paramSetHandle(0)
  {
  }

  /** @brief set the param set handle */
  void
    ParamSet::setParamSetHandle(OfxParamSetHandle h)
  {
    // set me handle
    _paramSetHandle = h;

    if(h) {
      // fetch me props
      OfxPropertySetHandle props;
      OfxStatus stat = OFX::Private::gParamSuite->paramSetGetPropertySet(h, &props);
      _paramSetProps.propSetHandle(props);
      throwSuiteStatusException(stat);
    }
    else {
      _paramSetProps.propSetHandle(0);
    }
  }

  /** @brief dtor */
  ParamSet::~ParamSet()
  {
    // delete any descriptor we may have constructed
    std::map<std::string, Param *>::iterator iter;
    for(iter = _fetchedParams.begin(); iter != _fetchedParams.end(); ++iter) {
      if(iter->second) {
        delete iter->second;
        iter->second = NULL;
      }
    }
  }

  /** @brief calls the raw OFX routine to fetch a param */
  void ParamSet::fetchRawParam(const std::string &name, ParamTypeEnum paramType, OfxParamHandle &handle) const
  {
    OfxPropertySetHandle propHandle;

    OfxStatus stat = OFX::Private::gParamSuite->paramGetHandle(_paramSetHandle, name.c_str(), &handle, &propHandle);
    throwSuiteStatusException(stat);

    PropertySet props(propHandle);

    // make sure it is of our type
    std::string paramTypeStr = props.propGetString(kOfxParamPropType);
    if(paramTypeStr != mapParamTypeEnumToString(paramType)) {
      std::string msg = std::string("Parameter \"") + name + std::string("\" exists but has type ") + paramTypeStr + std::string(" instead of ") + mapParamTypeEnumToString(paramType);
      throw OFX::Exception::TypeRequest(msg.c_str());
    }
  }

#ifdef OFX_EXTENSIONS_NUKE
  /** @brief calls the raw OFX routine to fetch a camera param */
  void ParamSet::fetchRawCameraParam(OfxImageEffectHandle pluginHandle, const std::string& name, NukeOfxCameraHandle& handle) const
  {
    OfxPropertySetHandle propHandle;

    OfxStatus stat = OFX::Private::gCameraParameterSuite->cameraGetHandle(pluginHandle, name.c_str(), &handle, &propHandle);

    throwSuiteStatusException( stat );
  }
#endif

  ParamTypeEnum ParamSet::getParamType(const std::string& name) const
  {
    OfxPropertySetHandle propHandle;
    OfxParamHandle handle;
    OfxStatus stat = OFX::Private::gParamSuite->paramGetHandle(_paramSetHandle, name.c_str(), &handle, &propHandle);
    throwSuiteStatusException(stat);
    PropertySet props(propHandle);
    // make sure it is of our type
    std::string paramTypeStr = props.propGetString(kOfxParamPropType);
    return mapParamTypeStringToEnum(paramTypeStr.c_str());
  }

  bool ParamSet::paramExists(const std::string& name) const
  {
    OfxParamHandle handle;
    OfxPropertySetHandle propHandle;
    OfxStatus stat = OFX::Private::gParamSuite->paramGetHandle(_paramSetHandle, name.c_str(), &handle, &propHandle);
    if(stat!=kOfxStatOK)
      return false;
    return true;
  }

  Param* ParamSet::getParam(const std::string& name) const
  {
    OfxParamHandle handle;
    OfxPropertySetHandle propHandle;
    OfxStatus stat = OFX::Private::gParamSuite->paramGetHandle(_paramSetHandle, name.c_str(), &handle, &propHandle);
    throwSuiteStatusException(stat);

    PropertySet props(propHandle);

    // make sure it is of our type
    std::string paramTypeStr = props.propGetString(kOfxParamPropType);
    ParamTypeEnum t = mapParamTypeStringToEnum(paramTypeStr.c_str());
    switch(t) 
    {
    case eStringParam :
      {
        StringParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case eIntParam :           
      {
        IntParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case eInt2DParam : 
      {
        Int2DParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case eInt3DParam :
      {
        Int3DParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case eDoubleParam : 
      {
        DoubleParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case eDouble2DParam : 
      {
        Double2DParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case eDouble3DParam : 
      {
        Double3DParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case eRGBParam : 
      {
        RGBParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case eRGBAParam : 
      {
        RGBAParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case eBooleanParam : 
      {
        BooleanParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case eChoiceParam :           
      {
        ChoiceParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case eCustomParam : 
      {
        CustomParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case eGroupParam :
      {
        GroupParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case ePageParam : 
      {
        PageParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case ePushButtonParam : 
      {
        PushButtonParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
    case eParametricParam : 
      {
        ParametricParam* ptr = 0;
        fetchParam(name, t, ptr);
        return ptr;
      }
#ifdef OFX_EXTENSIONS_NUKE
    case eCameraParam:
      {
        // You can't fetch a camera parameter from here...
        throwSuiteStatusException(kOfxStatErrFatal);
        //CameraParam* ptr = 0;
        //fetchParam(name, t, ptr);
        //return ptr;
      }
#endif
    default:
      assert(false);
    }
    return 0;
  }

  /** @brief if a param has been fetched in this set, go find it */
  Param *
    ParamSet::findPreviouslyFetchedParam(const std::string &name) const
  {
    // search
    std::map<std::string, Param *>::const_iterator search;
    search = _fetchedParams.find(name);
    if(search == _fetchedParams.end())
      return NULL;
    return search->second;
  }

  /** @brief Fetch an integer param */
  IntParam * 
    ParamSet::fetchIntParam(const std::string &name) const
  { 
    IntParam *param = NULL;
    fetchParam(name, eIntParam, param);
    return param;
  }

  /** @brief Fetch a 2D integer param */
  Int2DParam *ParamSet::fetchInt2DParam(const std::string &name) const
  {
    Int2DParam *param = NULL;
    fetchParam(name, eInt2DParam, param);
    return param;
  }

  /** @brief Fetch a 3D integer param */
  Int3DParam *ParamSet::fetchInt3DParam(const std::string &name) const
  {
    Int3DParam *param = NULL;
    fetchParam(name, eInt3DParam, param);
    return param;
  }

  /** @brief Fetch an double param */
  DoubleParam * 
    ParamSet::fetchDoubleParam(const std::string &name) const
  { 
    DoubleParam *param = NULL;
    fetchParam(name, eDoubleParam, param);
    return param;
  }

  /** @brief Fetch a 2D double param */
  Double2DParam *ParamSet::fetchDouble2DParam(const std::string &name) const
  {
    Double2DParam *param = NULL;
    fetchParam(name, eDouble2DParam, param);
    return param;
  }

  /** @brief Fetch a 3D double param */
  Double3DParam *ParamSet::fetchDouble3DParam(const std::string &name) const
  {
    Double3DParam *param = NULL;
    fetchParam(name, eDouble3DParam, param);
    return param;
  }

  /** @brief Fetch a string param */
  StringParam *ParamSet::fetchStringParam(const std::string &name) const
  {
    StringParam *param = NULL;
    fetchParam(name, eStringParam, param);
    return param;
  }

  /** @brief Fetch a RGBA param */
  RGBAParam *ParamSet::fetchRGBAParam(const std::string &name) const
  {
    RGBAParam *param = NULL;
    fetchParam(name, eRGBAParam, param);
    return param;
  }

  /** @brief Fetch an RGB  param */
  RGBParam *ParamSet::fetchRGBParam(const std::string &name) const
  {
    RGBParam *param = NULL;
    fetchParam(name, eRGBParam, param);
    return param;
  }

  /** @brief Fetch a Boolean  param */
  BooleanParam *ParamSet::fetchBooleanParam(const std::string &name) const
  {
    BooleanParam *param = NULL;
    fetchParam(name, eBooleanParam, param);
    return param;
  }

  /** @brief Fetch a Choice param */
  ChoiceParam *ParamSet::fetchChoiceParam(const std::string &name) const
  {
    ChoiceParam *param = NULL;
    fetchParam(name, eChoiceParam, param);
    return param;
  }

  /** @brief Fetch a group param */
  GroupParam *ParamSet::fetchGroupParam(const std::string &name) const
  {
    GroupParam *param = NULL;
    fetchParam(name, eGroupParam, param);
    return param;
  }

  /** @brief Fetch a Page param */
  PageParam *ParamSet::fetchPageParam(const std::string &name) const
  {
    PageParam *param = NULL;
    fetchParam(name, ePageParam, param);
    return param;
  }

  /** @brief Fetch a push button  param */
  PushButtonParam *ParamSet::fetchPushButtonParam(const std::string &name) const
  {
    PushButtonParam *param = NULL;
    fetchParam(name, ePushButtonParam, param);
    return param;
  }

  /** @brief Fetch a custom param */
  CustomParam *ParamSet::fetchCustomParam(const std::string &name) const
  {
    CustomParam *param = NULL;
    fetchParam(name, eCustomParam, param);
    return param;
  }

  /** @brief Fetch a parametric param */
  ParametricParam *ParamSet::fetchParametricParam(const std::string &name) const
  {
    ParametricParam *param = NULL;
    fetchParam(name, eParametricParam, param);
    return param;
  }

  /// open an undoblock
  void ParamSet::beginEditBlock(const std::string &name)
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramEditBegin(_paramSetHandle, name.c_str());
    throwSuiteStatusException(stat);
  }

  /// close an undoblock
  void ParamSet::endEditBlock()
  {
    OfxStatus stat = OFX::Private::gParamSuite->paramEditEnd(_paramSetHandle);
    throwSuiteStatusException(stat);
  }

};
