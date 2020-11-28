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

#ifndef __OPENSPACE_CORE___CONVERTRECFILEVERSIONTASK___H__
#define __OPENSPACE_CORE___CONVERTRECFILEVERSIONTASK___H__

#include <openspace/util/task.h>
#include <openspace/interaction/sessionrecording.h>

#include <ghoul/glm.h>

#include <string>

namespace openspace::interaction {

class ConvertRecFileVersionTask : public Task {
public:
    ConvertRecFileVersionTask(const ghoul::Dictionary& dictionary);
    std::string description() override;
    void perform(const Task::ProgressCallback& progressCallback) override;
    static documentation::Documentation documentation();
    void convert();

private:
    std::string _inFilename;
    std::string _inFilePath;
    std::string _valueFunctionLua;
};

} // namespace openspace::interaction

#endif //__OPENSPACE_CORE___CONVERTRECFILEVERSIONTASK___H__