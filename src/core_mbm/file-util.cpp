/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#include <util-interface.h>
#include <cstring>
#include <string>
#include <cr-static-local.h>

#define GetCurrentDir getcwd

#ifdef _WIN32
    #include <io.h>
    #include <dirent-1-13/dirent.h>
    #include <direct.h>
    #pragma warning(disable : 4996) //access
#elif defined ANDROID
    #include <platform/common-jni.h>
    #include <android/asset_manager.h>
    #include <android/log.h>
    #include <jni.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <errno.h>
#elif __linux__
    #include <climits>
    #include <cstdarg>
    #include <unistd.h>
    #include <dirent.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <errno.h>
#endif

#include <random>

std::vector<std::string> lsPath;
std::string              pathRet;
OnAddPathScript onAddPathScript = nullptr;

#ifdef _WIN32
    std::string currentDir;
#endif
std::string auxRet_1,auxRet_2,auxRet_3;

#ifdef _WIN32
        const char *_ERROR_DIRSEPARATOR = "/";
#else
        const char *_ERROR_DIRSEPARATOR = "\\";
#endif

#ifdef _WIN32
        std::string ___getDecompressModelFileName()
        {
            std::string decompressModelFile;
            char        str[255] = "";
            if (GetTempPathA(sizeof(str), str))
            {
                decompressModelFile = str;
                decompressModelFile += "decompressModel.tmp";
            }
            else
            {
                decompressModelFile = "decompressModel.tmp";
            }
            return decompressModelFile;
    }
#elif defined ANDROID
    const char* ___getDecompressModelFileName()
    {
        return "decompressModel.tmp";
    }
#elif defined __linux__
    const char* ___getDecompressModelFileName()
    {
        return "/tmp/decompressModel.tmp";
    }
#else
    #error "Unknown platform"
#endif

static const char * getCorrectSeparator2SO(const char *fileName)
{
    if (fileName && strstr(fileName, _ERROR_DIRSEPARATOR))
    {
        std::string out;
        static char f[1024] = "";

        log_util::repalceDefaultSeparator(fileName, out);
        strncpy(f, out.c_str(),sizeof(f)-1);
        return f;
    }
    return fileName;
}

const char * getNameOnly(const char *fileNamePath)
{
    if (fileNamePath == nullptr)
        return nullptr;
    auxRet_1.clear();
    std::vector<std::string> lsRet;
    fileNamePath = getCorrectSeparator2SO(fileNamePath);
    util::split(lsRet, fileNamePath, util::getDirSeparator()[0]);
    if (lsRet.size())
    {
        const size_t s = lsRet.size() - 1;
        auxRet_1                  = lsRet[s];
        return auxRet_1.c_str();
    }
    else
    {
        return fileNamePath;
    }
}

static const char * justPath(const char *path)
{
    if (path == nullptr)
        return nullptr;
    path = getCorrectSeparator2SO(path);
    std::string        str(path);
    auxRet_2.clear();
    if (str.find('.') == std::string::npos)
        auxRet_2 = std::move(str);
    return auxRet_2.c_str();
}

static const char * getPathFromName(const char *fileNamePath)
{
    if (fileNamePath == nullptr)
        return nullptr;
    fileNamePath = getCorrectSeparator2SO(fileNamePath);
    auxRet_3.clear();
    std::vector<std::string> lsRet;
    util::split(lsRet, fileNamePath, util::getCharDirSeparator());
    if (lsRet.size())
    {
        const size_t s = lsRet.size() - 1;
        for (size_t i = 0; i < s; ++i)
        {
            auxRet_3 += lsRet[i];
            if ((i + 1) < s)
                auxRet_3 += util::getCharDirSeparator();
        }
        if (s)
        {
            std::string last(lsRet[s]);
            util::split(lsRet, last.c_str(), '.');
            if (lsRet.size() <= 1)
            {
                auxRet_3 += util::getCharDirSeparator();
                auxRet_3 += last;
            }
        }
    }
    if (auxRet_3.size() == 0)
    {
        return justPath(fileNamePath);
    }
    return auxRet_3.c_str();
}

static const char * concatPath(const char *path, std::string &currentPathToAdd, std::string &newPathOut)
{
    if (path && strlen(path))
    {
        std::string strPath(path);
        if (strPath[0] == util::getCharDirSeparator())
            strPath.erase(0, 1);
        if (strPath[strPath.size() - 1] == util::getCharDirSeparator())
        {
            newPathOut = currentPathToAdd;
            newPathOut += strPath;
            return newPathOut.c_str();
        }
        else
        {
            newPathOut = currentPathToAdd;
            newPathOut += util::getCharDirSeparator();
            newPathOut += strPath;
            return newPathOut.c_str();
        }
    }
    return nullptr;
}




namespace util
{
    void remove_directory(const char * folder)
    {
        if(folder)
        {
            DIR *dirp = nullptr;
            struct dirent *dp = nullptr;
            if ((dirp = opendir(folder)) == nullptr) 
	        {
                PRINT_IF_DEBUG("couldn't open '%s'",folder);
                return;
            }
            do {
                errno = 0;
                if ((dp = readdir(dirp)) != nullptr) 
		        {
			        if (dp->d_type == DT_DIR && (strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0))
			        {
				        std::string str(folder);
				        str += "/";
				        str += dp->d_name;
				        remove_directory(str.c_str());
			        }
			        else if(dp->d_type == DT_REG && strcmp(folder,".") != 0)
			        {
                        std::string str(folder);
				        str += "/";
				        str += dp->d_name;
                        remove(str.c_str());
			        }
                }
            }while (dp != nullptr);
            closedir(dirp);
            #ifdef _WIN32
            _rmdir(folder);
            #else
            rmdir(folder);
            #endif
        }
    }

    bool directoy_exists(const char * folder_name)
    {
        if (folder_name == nullptr)
            return false;
        const char *tmpPath = getCorrectSeparator2SO(folder_name);
        DIR *       dir     = opendir(tmpPath);
        if (dir)
        {
            closedir(dir);
            return true;
        }
        return false;
    }

    bool create_tmp_directoy(const char * folder_base_name, char* folder_name_output, const int size_folder_name_output)
    {
        if(folder_base_name == nullptr) // will create a tmp folder
        {
            #if defined          ANDROID
                util::COMMON_JNI *cJni        = util::COMMON_JNI::getInstance();
                const char *     currentPath  = cJni->absPath.c_str();
                char template_name[255]       = "";
                if(cJni->absPath.size() > 0 && cJni->absPath[cJni->absPath.size()-1] == '/')
                    snprintf(template_name,sizeof(template_name),"%sasset_XXXXXX",currentPath);
                else
                    snprintf(template_name,sizeof(template_name),"%s/asset_XXXXXX",currentPath);
                char *dir_name = mkdtemp(template_name);
            #elif defined _WIN32
                auto getRandomTmpFolder  =[] () ->std::string
                {
                    std::random_device dev;
                    std::mt19937 rng(dev());
                    const char alphanum[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
                    const int string_length = sizeof(alphanum)-1;
                    std::uniform_int_distribution<std::mt19937::result_type> dist6(0,string_length-1);
                    const int size_folder_name = 6;
                    std::string folder_name;

                    for(int i = 0; i < size_folder_name; i++)
                    {
                        const int index   = dist6(rng);//alphanum[dist6(rng) % size_pass];
                        const char letter = alphanum[index];
                        folder_name.push_back(letter);
                    }
                    return folder_name;
                };
                char dir_name[255]            = "";
                char lpBuffer[255]            = "";
                const std::string tmp_folder_name(getRandomTmpFolder());
                if(GetTempPathA(sizeof(lpBuffer),lpBuffer) != 0 )
                {
                    const int len = strlen(lpBuffer);
                    if(len > 0 && lpBuffer[len-1] == '\\' )
                        lpBuffer[len-1] = 0;
                    snprintf(dir_name,sizeof(dir_name),"%s\\%s",lpBuffer,tmp_folder_name.c_str());
                }
                else
                {
                    snprintf(dir_name,sizeof(dir_name),"%s",tmp_folder_name.c_str());
                }
                if (_mkdir(dir_name) != 0 && directoy_exists(dir_name) == false)
                {
                    ERROR_LOG("Failed to create folder:\n%s",dir_name);
                    return false;
                }
            #else
                char template_name[] = "/tmp/asset_XXXXXX";
                char *dir_name = mkdtemp(template_name);
            #endif
            if(dir_name)
            {
                util::addPath(dir_name);
                if(folder_name_output)
                {
                    snprintf(folder_name_output,size_folder_name_output,"%s",dir_name);
                }
                return true;
            }
            else
            {
                ERROR_LOG("%s","mkdtemp returned NULL");
                return false;
            }
        }
        else//base name folder is given
        {
            auto fHasSeparator = [] (const char * sPath) -> bool
            {
                std::string path(sPath ? sPath : "");
                if(path.find('\\') != std::string::npos)
                    return true;
                if(path.find('/') != std::string::npos)
                    return true;
                return false;
            };

            auto fCreateFolder = [] (const char * sPath) -> bool
            {
                #if defined _WIN32
                if (_mkdir(sPath) != 0 && directoy_exists(sPath) == false)
                #else
                if (mkdir(sPath,0777) != 0 && directoy_exists(sPath) == false)
                #endif
                {
                    const char *  pSError = strerror(errno);
                    ERROR_LOG("Failed creating folder [%s] [%s]",sPath,pSError ? pSError : "");
                    return false;
                }
                else
                {
                    util::addPath(sPath);
                    return true;
                }
            };

            if(directoy_exists(folder_base_name))
            {
                if(folder_name_output)
                {
                    snprintf(folder_name_output,size_folder_name_output,"%s",folder_base_name);
                }
                return true;
            }
            else if (fHasSeparator(folder_base_name) == false)//is not full path
            {
                char dir_name[255]            = "";
                #if defined          ANDROID
                util::COMMON_JNI *cJni        = util::COMMON_JNI::getInstance();
                const char *     currentPath  = cJni->absPath.c_str();
                if(cJni->absPath.size() > 0 && cJni->absPath[cJni->absPath.size()-1] == '/')
                    snprintf(dir_name,sizeof(dir_name),"%s%s",currentPath,folder_base_name);
                else
                    snprintf(dir_name,sizeof(dir_name),"%s/%s",currentPath,folder_base_name);
                cJni->addPathDroid(dir_name);
                #elif defined _WIN32
                char lpBuffer[255]            = "";
                if(GetTempPathA(sizeof(lpBuffer),lpBuffer) != 0 )
                {
                    const int len = strlen(lpBuffer);
                    if(len > 0 && lpBuffer[len-1] == '\\' )
                        lpBuffer[len-1] = 0;
                    snprintf(dir_name,sizeof(dir_name),"%s\\%s",lpBuffer,folder_base_name);
                }
                else
                {
                    snprintf(dir_name,sizeof(dir_name),"%s",folder_base_name);
                }
                #else
                snprintf(dir_name,sizeof(dir_name),"/tmp/%s",folder_base_name);
                #endif

                if (fCreateFolder(dir_name) == false)
                {
                    return false;
                }
                else
                {
                    util::addPath(dir_name);
                }
                if(folder_name_output)
                {
                    snprintf(folder_name_output,size_folder_name_output,"%s",dir_name);
                }
                return true;
            }
            else//it is full path
            {
                if (fCreateFolder(folder_base_name) == false)
                {
                    return false;
                }
                else
                {
                    util::addPath(folder_base_name);
                    if(folder_name_output)
                    {
                        snprintf(folder_name_output,size_folder_name_output,"%s",folder_base_name);
                    }
                    return true;
                }
            }
        }
    }

    FILE *fopenApp(const char *fileName, const char *mode)
    {
#if defined ANDROID
        if (mode && strchr(mode, 'w'))
        {
            util::COMMON_JNI *commonJni = util::COMMON_JNI::getInstance();
            const char *currentPath = commonJni->absPath.c_str();
            if (currentPath)
            {
                std::string file(currentPath);
                file += "/";
                file += fileName;
                FILE *fp = fopen(file.c_str(), mode);
                if (!fp)
                {
                    PRINT_IF_DEBUG("Failed to open file to write mode\n[%s]", file.c_str());
                }
                return fp;
            }
            else
            {
                PRINT_IF_DEBUG("current Path not set. An error will occurs [probability]\nAndroid needs to have full path to create file. ");
            }
            return fopen(fileName, mode);
        }
        else
        {
            return fopen(fileName, mode);
        }
#else
        return fopen(fileName, mode);
#endif
    }

    void removeAppFile(const char *fileName)
    {
        remove(fileName);
    }

    FILE* openFile(const char *fileName, const char *mode)
    {
        if (fileName && mode)
        {
            fileName = getCorrectSeparator2SO(fileName);
            if (strncmp(mode, "r", 1) == 0)
            {
                const char *fullFileName = getFullPath(fileName,nullptr);
                FILE *      fp           = util::fopenApp(fullFileName, mode);
                if (fp)
                    addPath(fullFileName);
                return fp;
            }
            else
            {
                FILE *fp = util::fopenApp(fileName, mode);
                if (fp)
                    addPath(fileName);
                return fp;
            }
        }
        return nullptr;
    }

    const char* getBaseName(const char *fileName)
    {
        static std::string ret;
        ret.clear();
        if(fileName)
        {
            std::string f(fileName);
            std::string::size_type  p = f.find_last_of('\\');
            if(p != std::string::npos)
            {
                std::string f2(f.substr(p));
                std::string::size_type  p2 = f2.find_last_of('/');
                if(p2 != std::string::npos)
                {
                    
                    ret= std::string(&fileName[p2+1]);

                }
                else
                {
                    ret = std::string(&fileName[p+1]);
                }
            }
            else
            {
                std::string f2(fileName);
                std::string::size_type  p2 = f2.find_last_of('/');
                if(p2 != std::string::npos)
                {
                    ret = std::string(&fileName[p2+1]);
                }
                else
                {
                    ret = std::string(fileName);
                }
            }
        }
        return ret.c_str();
    }

#ifdef ANDROID
    const char * getFullPath(const char *fileName, bool *existPath )
    {
        if (fileName == nullptr)
            return nullptr;
		if(strlen(fileName) == 0)
			return fileName;
        if(strstr(fileName,util::getDecompressModelFileName()) != nullptr)
        {
            util::COMMON_JNI *commonJni = util::COMMON_JNI::getInstance();
            const char *currentPath = commonJni->absPath.c_str();
            if (currentPath)
            {
                static std::string fileDecompress;
                if(fileDecompress.size() == 0)
                {
                    fileDecompress += currentPath;
                    fileDecompress += "/";
                    fileDecompress += util::getDecompressModelFileName();
                }
                if (existPath)
                    *existPath              = true;
                return fileDecompress.c_str();
            }
        }
        fileName                  = getCorrectSeparator2SO(fileName);
        util::COMMON_JNI *commonJni = util::COMMON_JNI::getInstance();
        if (access_file(fileName, 0) != 0)
        { // file doesnt exist
            const char *nameOnly = getNameOnly(fileName);
            if (access_file(nameOnly, 0) == 0)
            {
                if (existPath)
                    *existPath              = true;
                const char *fileNameAndorid = commonJni->copyFileFromAsset(fileName, "r");
                if (fileNameAndorid)
                {
                    addPath(fileNameAndorid);
                    return fileNameAndorid;
                }
                else
                {
                    nameOnly = commonJni->copyFileFromAsset(nameOnly, "r");
                    if (nameOnly)
                    {
                        addPath(nameOnly);
                        return nameOnly;
                    }
                    else
                    {
                        addPath(fileName);
                        return fileName;
                    }
                }
            }
            for (uint32_t i = 0; i < lsPath.size(); ++i)
            {
                std::string fullPath(lsPath[i]);
                fullPath += util::getCharDirSeparator();
                fullPath += nameOnly;
                if (access_file(fullPath.c_str(), 0) == 0)
                {
                    if (existPath)
                        *existPath              = true;
                    pathRet               = fullPath;
                    const char *fileNameAndorid = commonJni->copyFileFromAsset(fullPath.c_str(), "r");
                    if (fileNameAndorid)
                    {
                        addPath(fileNameAndorid);
                        return fileNameAndorid;
                    }
                    else
                    {
                        static std::string s_fullPath;
                        s_fullPath = fullPath;
                        addPath(s_fullPath.c_str());
                        return s_fullPath.c_str();
                    }
                }
            }
            if (existPath)
                *existPath = false;
            return nameOnly ? nameOnly : fileName;
        }
        else
        {
            if (existPath)
                *existPath              = true;
            const char *fileNameAndorid = commonJni->copyFileFromAsset(fileName, "r");
            if (fileNameAndorid)
            {
                addPath(fileNameAndorid);
                return fileNameAndorid;
            }
            else
            {
                addPath(fileName);
                return fileName;
            }
        }
    }
#else
    const char * getFullPath(const char *fileName, bool *existsFile )
    {
        if (fileName == nullptr)
            return nullptr;
		if (fileName[0] == 0)
			return fileName;
        fileName = getCorrectSeparator2SO(fileName);
        if (access_file(fileName, 0) != 0 || strstr(fileName, util::getDirSeparator()) == nullptr)
        {
            const char *nameOnly = getNameOnly(fileName);
            if (access_file(nameOnly, 0) == 0 && strstr(fileName, util::getDirSeparator()))
            {
                if (existsFile)
                    *existsFile = true;
                addPath(nameOnly);
                return nameOnly;
            }
            for (auto fullPath : lsPath)
            {
                fullPath += util::getCharDirSeparator();
                fullPath += nameOnly;
                if (access_file(fullPath.c_str(), 0) == 0)
                {
                    if (existsFile)
                        *existsFile = true;
                    pathRet  = fullPath;
                    addPath(fullPath.c_str());
                    return pathRet.c_str();
                }
            }
            if (existsFile)
                *existsFile = false;
            for (auto fullPath : lsPath)
            {
                fullPath += util::getCharDirSeparator();
                fullPath += fileName;
                if (access_file(fullPath.c_str(), 0) == 0)
                {
                    if (existsFile)
                        *existsFile = true;
                    pathRet  = fullPath;
                    addPath(fullPath.c_str());
                    return pathRet.c_str();
                }
            }
            return fileName;
        }
        else
        {
            if (existsFile)
                *existsFile = true;
            addPath(fileName);
            return fileName;
        }
    }
#endif

	void setOnAddPathScript(OnAddPathScript onNewAddPathScript) noexcept
	{
		onAddPathScript = onNewAddPathScript;
	}

    void addPath(const char *newPathSource)
    {
		
        if (newPathSource)
        {
			std::string newPathBuffer;
            newPathSource    = getCorrectSeparator2SO(newPathSource);
            const char *path = getPathFromName(newPathSource);
			if(onAddPathScript)
				onAddPathScript(path);
    #ifdef ANDROID // add anyway bacause we are going to search in the files of Android
            util::COMMON_JNI *jni = util::COMMON_JNI::getInstance();
            if (jni)
            {
                jni->addPathDroid(path);
                for (uint32_t i = 0; i < lsPath.size(); ++i)
                {
                    if (lsPath[i].compare(path) == 0) // Ja existe este path
                    {
                        return;
                    }
                }
                lsPath.push_back(path);
                return;
            }
    #endif
            if (path && strlen(path))
            {
                if (!directoy_exists(path))
                {
                    for (auto currentPathToadd : lsPath)
                    {
                        const char *tmpPath = concatPath(path, currentPathToadd, newPathBuffer);
                        if (directoy_exists(tmpPath))
                        {
                            path = tmpPath;
                            break;
                        }
                    }
                }

                for (auto & i : lsPath)
                {
                    if (i.compare(path) == 0)
                    {
                        return;
                    }
                }

                DIR *dir = opendir(path);
                if (dir == nullptr)
                {
					char cCurrentPath[1024] = "";
                    if (GetCurrentDir(cCurrentPath, sizeof(cCurrentPath)))
                    {
                        const size_t len = strlen(cCurrentPath);
                        if (len > 0)
                        {
                            if (cCurrentPath[len - 1] == util::getCharDirSeparator())
                            {
                                strncat(cCurrentPath, path, sizeof(cCurrentPath) - strlen(cCurrentPath) - 1);
                            }
                            else
                            {
                                std::string tmpcCurrentPath(cCurrentPath);
                                tmpcCurrentPath.push_back(util::getCharDirSeparator());
                                tmpcCurrentPath += path;
                                strncpy(cCurrentPath, tmpcCurrentPath.c_str(), sizeof(cCurrentPath)-1);
                            }
                        }
                        else
                        {
                            strncpy(cCurrentPath, path, sizeof(cCurrentPath) - 1);
                        }
                    }
                    else
                    {
                        strncpy(cCurrentPath, path, sizeof(cCurrentPath) - 1);
                    }
                    dir = opendir(cCurrentPath);
                    for (std::vector<std::string>::size_type i = 0; i < lsPath.size() && dir == nullptr; ++i)
                    {
                        std::vector<std::string>::size_type len = lsPath[i].size();
                        if (len)
                        {
                            const char *_path = lsPath[i].c_str();
                            if (_path[len - 1] == util::getCharDirSeparator())
                            {
                                sprintf(cCurrentPath, "%s%s", _path, path);
                            }
                            else
                            {
                                sprintf(cCurrentPath, "%s%c%s", _path, util::getCharDirSeparator(), path);
                            }
                            dir = opendir(cCurrentPath);
                        }
                    }
                    if (dir)
                        path = cCurrentPath;
                }
                if (dir)
                {
                    closedir(dir);
                    for (auto & i : lsPath)
                    {
                        if (i.compare(path) == 0)
                        {
                            return;
                        }
                    }
                    lsPath.emplace_back(path);
                }
            }
        }
    }

    bool getSizeFile(FILE *fp, size_t *sizeOut)
    {
        if (fp == nullptr)
            return false;
        if (fseek(fp, 0, SEEK_SET))
            return log_util::fail(__LINE__, __FILE__, "\nerror to read file seek_set = 0");
        if (fseek(fp, 0, SEEK_END))
            return log_util::fail(__LINE__, __FILE__, "\nerror to read file seek_end");
        const long t = ftell(fp);
        if (t <= -1)
            return log_util::fail(__LINE__, __FILE__, "\nerror to read file size <= -1");
        if (t == 0)
            return log_util::fail(__LINE__, __FILE__, "\nerror to read file size =0");
        *sizeOut = static_cast<size_t>(t);
        if (fseek(fp, 0, SEEK_SET))
            return log_util::fail(__LINE__, __FILE__, "\nerror to read file seek_set = 0");
        return true;
    }

    const char * existFile(const char *fileName)
    {
        bool existsFile = false;
        if (fileName == nullptr)
            return nullptr;
        if (access_file(fileName, 0) == 0)
            return fileName;
        fileName                   = getCorrectSeparator2SO(fileName);
        const char *  fullFileName = getFullPath(fileName,&existsFile);
        if (existsFile)
            return fullFileName;
        return nullptr;
    }

    bool saveToFileBinary(const char *fileName,const  void *header,const  size_t sizeOfHeader,const  void *dataIn,const size_t sizeOfDataIn, FILE **fileOut)
    {
        if (fileName == nullptr)
            return false;
        if (header == nullptr)
            return false;
        if (!sizeOfHeader)
            return false;
        FILE *file = openFile(fileName, "wb");
        if (!file)
            return false;
        if (!fwrite(header, sizeOfHeader, 1, file))
        {
            fclose(file);
            file = nullptr;
            if (fileOut)
                *fileOut = nullptr;
            return false;
        }
        if (dataIn && sizeOfDataIn)
        {
            if (fwrite(dataIn, sizeOfDataIn, 1, file))
            {
                if (fileOut)
                    *fileOut = file;
                else
                    fclose(file);
                return true;//Opened File never closed. ok 
            }
            else
            {
                fclose(file);
                file = nullptr;
                if (fileOut)
                    *fileOut = nullptr;
                return false;
            }
        }
        if (fileOut)
            *fileOut = file;
        else
            fclose(file);
        return true;//Opened File never closed. ok 
    }

    bool addToFileBinary(const char *fileName,const void *dataIn,const  size_t sizeOfDataIn, FILE **fileOutOpened)
    {
        if (fileName == nullptr)
            return false;
        if (dataIn == nullptr)
            return false;
        if (!sizeOfDataIn)
            return false;
        if (fileOutOpened == nullptr)
            return false;
        if (*fileOutOpened == nullptr)
        {
            FILE *file = openFile(fileName, "wb");
            if (!file)
                return false;
            if (fwrite(dataIn, sizeOfDataIn, 1, file))
            {
                *fileOutOpened = file;//Opened File never closed. ok 
                return true;
            }
            fclose(file);
            file = nullptr;
            return false;
        }
        else
        {
            FILE *file = *fileOutOpened;
            if (file)
            {
                if (fwrite(dataIn, sizeOfDataIn, 1, file))
                {
                    return true;
                }
                fclose(file);
                file           = nullptr;
                *fileOutOpened = nullptr;
                return false;
            }
        }
        return true;
    }

    void getAllPaths(std::vector<std::string> &lsPathOut)
    {
        lsPathOut.clear();
        for (auto & i : lsPath)
        {
            lsPathOut.push_back(i);
        }
    }

    const char * getDecompressModelFileName() 
    {
        CR_DEFINE_STATIC_LOCAL_ARG(std::string,decompressModelFile,___getDecompressModelFileName());
        return decompressModelFile.c_str();
    }
}
