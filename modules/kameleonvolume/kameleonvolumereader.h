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

#ifndef __OPENSPACE_MODULE_KAMELEONVOLUME___KAMELEONVOLUMEREADER___H__
#define __OPENSPACE_MODULE_KAMELEONVOLUME___KAMELEONVOLUMEREADER___H__

#include <string>
#include <memory>
#include <modules/volume/rawvolume.h>
#include <ghoul/misc/dictionary.h>

#ifdef WIN32
#pragma warning (push)
#pragma warning (disable : 4619) // #pragma warning: there is no warning number '4619'
#pragma warning (disable : 4675) // #pragma warning: there is no warning number '4675'
#pragma warning (disable : 4800) // #pragma warning: there is no warning number '4800'
#endif // WIN32

#include <ccmc/Kameleon.h>
#include <ccmc/Interpolator.h>

#ifdef WIN32
#pragma warning (pop)
#endif // WIN32

namespace ccmc { class Model; }

namespace openspace {
namespace kameleonvolume {

class KameleonVolumeReader {
public:
    KameleonVolumeReader(const std::string& path);

    std::unique_ptr<volume::RawVolume<float>> readFloatVolume(
        const glm::uvec3& dimensions,
        const std::string& variable,
        const glm::vec3& lowerDomainBound,
        const glm::vec3& upperDomainBound) const;

    std::unique_ptr<volume::RawVolume<float>> readFloatVolume(
        const glm::uvec3& dimensions,
        const std::string& variable,
        const glm::vec3& lowerBound,
        const glm::vec3& upperBound,
        float& minValue,
        float& maxValue) const;

    ghoul::Dictionary readMetaData() const;

    std::string time() const;
    std::string simulationStart() const;
    std::string simulationEnd() const;
    float elapsedTime() const;

    double minValue(const std::string& variable) const;
    double maxValue(const std::string& variable) const;

    std::vector<std::string> gridVariableNames() const;
    std::vector<std::string> variableNames() const;
    std::vector<std::string> variableAttributeNames() const;
    std::vector<std::string> globalAttributeNames() const;

private:
    static void addAttributeToDictionary(ghoul::Dictionary& dictionary, const std::string& key, ccmc::Attribute& attr);
    std::string _path;
    ccmc::Kameleon _kameleon;
    ccmc::Model* _model;
    std::unique_ptr<ccmc::Interpolator> _interpolator;

};

} // namespace kameleonvolume
} // namespace openspace

#endif // __OPENSPACE_MODULE_KAMELEONVOLUME___KAMELEONVOLUMEREADER___H__
