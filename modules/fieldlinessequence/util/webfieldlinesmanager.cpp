/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2019                                                               *
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

#include <modules/fieldlinessequence/util/webfieldlinesmanager.h>

#include <ghoul/logging/logmanager.h>
#include <openspace/util/httprequest.h>
#include <modules/sync/syncs/httpsynchronization.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/filesystem/file.h>
#include <openspace/util/timemanager.h>
#include <openspace/engine/globals.h>


namespace {
    constexpr const char* _loggerCat = "FieldlinesSequence[ Web FLs Manager ]";
    
} // namepace


namespace openspace{


    
    
    // --------------------------- PUBLIC FUNCTIONS --------------------------- //
    void WebFieldlinesManager::initializeWebFieldlinesManager(std::string identifier, std::string fieldLineModelType, size_t& _nStates, std::vector<std::string>& _sourceFiles,
                                                              std::vector<double>& _startTimes)
    {
        if (_connected) {
            _flsType = fieldLineModelType;

            // Initialize the sliding window
            _webFieldlinesWindow = WebFieldlinesWindow(_syncDir, "http://localhost:3000/", _sourceFiles, _startTimes, _nStates, convertIdentifierToID(identifier));


            LERROR("WebFieldlinesManager initialized " + identifier);
        }        
    }

    bool WebFieldlinesManager::checkConnectionToServer() {
        SyncHttpMemoryDownload mmryDld = SyncHttpMemoryDownload("http://localhost:3000/");
        HttpRequest::RequestOptions opt = {};
        opt.requestTimeoutSeconds = 0;
        mmryDld.download(opt);
        std::string s = "";
        std::transform(mmryDld.downloadedData().begin(), mmryDld.downloadedData().end(), std::back_inserter(s),
            [](char c) {
            return c;
        });
        if (s == "You are at ROOT") {
            _connected = true;
            return true;
        }
        else
            return false;
    }

    bool WebFieldlinesManager::isConnected() {
        return _connected;
    }
    
    // Make sure that the sync directory exists
    // Also creates a new directory in the web_fieldlines directory corresponding to the field line identifier
    std::string WebFieldlinesManager::initializeSyncDirectory(std::string identifier) {
        std::string path = absPath("${BASE}/sync/http/web_fieldlines") + FileSys.PathSeparator;
        
        if (!FileSys.directoryExists(path)) {
            FileSys.createDirectory(path);
        }
        path = absPath(path + identifier);
        if(!FileSys.directoryExists(path)) {
            FileSys.createDirectory(path);
        }
        
        _syncDir = path;
        return _syncDir;
    }
    
    // Temporary function - this should be moved to the worker. It's to download
    // the start latest line
    // this could be used to test the connection too
    void WebFieldlinesManager::preDownload(){
        // for some reason the fieldlines looks f-ed up when downloaded and read fromt his endpoint???? so weird
        // could it have something to do with endianness?
        //std::string url = "http://localhost:3000/WSA/fieldline/WSA_Fieldlines_PFSS_IO/2019-05-02T20-00-00.000.osfls";
        std::string url = "http://localhost:3000/WSA/PfssIo2019-05-02T20-00-00.000.osfls"; // temp thing, should be the latest one
        std::string destinationpath = absPath(_syncDir + ghoul::filesystem::FileSystem::PathSeparator + "2019-05-02T20-00-00.000.osfls"); // what the downloaded filename is to be
        AsyncHttpFileDownload ashd = AsyncHttpFileDownload(url, destinationpath, HttpFileDownload::Overwrite::Yes);
        HttpRequest::RequestOptions opt = {};
        opt.requestTimeoutSeconds = 0;
        ashd.start(opt);
        ashd.wait();
        if(ashd.hasSucceeded() == true ){
            LERROR("succeeeded: " + destinationpath);
        }
        if(ashd.hasFailed() == true ){
            LERROR("failed: " + destinationpath);
        }
    }
    
    
    void WebFieldlinesManager::update(){
        const double openspaceTime = global::timeManager.time().j2000Seconds();
        const auto deltaTime = global::timeManager.deltaTime();
        const int speedThreshhold = 10000;
        
        // Hold your horses, we don't want to do anything while deltatime is too high
        if (abs(deltaTime) < speedThreshhold){
            

            // First it checks the time against the "bigger window" aka the long list of
            // timesteps we know are available online. If it's outside that we're gonna need a new one
            if (_webFieldlinesWindow.timeIsInTriggerTimesWebList(openspaceTime) && !_webFieldlinesWindow.expectedWindowIsOutOfBounds(openspaceTime)) {

                // Check if in window
                if (_webFieldlinesWindow.timeIsInWindow(openspaceTime)) {

                    // Check if in the edge of the window, so we can start downloading a new one
                    if (_webFieldlinesWindow.timeIsInWindowMargin(openspaceTime, deltaTime)) {
                        // get new window
                        _webFieldlinesWindow.newWindow(openspaceTime);
                        hasUpdated = false;
                    }
                    else {
                        
                        // If it's in the middle of the window, we can just sit back and relax
                        // And let the worker work
                        _webFieldlinesWindow.executeDownloadWorker();
                    }
                }
                else {
                    // get new window
                    _webFieldlinesWindow.newWindow(openspaceTime);
                    hasUpdated = false;
                }
            }
            else {
                _webFieldlinesWindow.getNewTriggerTimesWebList(openspaceTime);
            }
        }        
    }

    bool WebFieldlinesManager::checkIfWindowIsReadyToLoad()
    {
        return _webFieldlinesWindow.workerWindowIsReady();
    }

    void WebFieldlinesManager::resetWorker() {
        _webFieldlinesWindow.rfsHasUpdated();
    }
    
    // --------------------------- PRIVATE FUNCTIONS --------------------------- //
    
    FieldLineType WebFieldlinesManager::convertIdentifierToID(std::string identifier) {
        std::map<std::string, FieldLineType> string2ID{
            {"WSA_Fieldlines_Sun_Earth_Connection",WSA_Fieldlines_Sun_Earth_Connection},
            {"WSA_Fieldlines_SCS_OI",WSA_Fieldlines_SCS_OI},
            {"WSA_Fieldlines_PFSS_IO",WSA_Fieldlines_PFSS_IO},
            {"WSA_Fieldlines_PFSS_OI",WSA_Fieldlines_PFSS_OI}
        };
        if (auto found = string2ID.find(identifier); found != string2ID.end()) {
            return found->second;
        }
        else
            return FieldLineType::ID_NOT_FOUND;
    }
    

    std::string WebFieldlinesManager::getDirectory(){
        return _syncDir;
    }


    
} // namespace openspace
