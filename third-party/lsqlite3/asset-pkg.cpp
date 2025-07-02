
#if defined _WIN32
    #define _SEPARATOR_ '\\'
    #include "windows.h"
    #include <io.h>
    #include <direct.h>
    #include <dirent-1-13/dirent.h>
#else
    #define  _POSIX_C_SOURCE 200809L
    #define _SEPARATOR_ '/'
    #include <stdlib.h>
    #include <unistd.h>
    #include <stdio.h>
    #include <dirent.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <errno.h>
#endif

#if defined          ANDROID
    #include <platform/common-jni.h>
#endif

#include "sqlite3ext.h"
#include <core_mbm/util-interface.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>

SQLITE_EXTENSION_INIT3

static std::vector<std::string> paths;


void addPathToEngine(const char * sPath)
{
    #if defined ANDROID
        util::COMMON_JNI *cJni        = util::COMMON_JNI::getInstance();
        cJni->addPathDroid(sPath);
    #endif
    paths.push_back(sPath);
    util::addPath(sPath);
}

static void listFilesFromFolder(const char * path, std::vector<std::string> & fileList)
{
    std::string folder(path);
    if(folder.size() > 0 && (folder[folder.size()-1] == '\\' ||  folder[folder.size()-1] == '/'))
        folder.pop_back();
    path = folder.c_str();
    DIR * dirp         = opendir(path);
    struct dirent * dp = nullptr;
    if (dirp) 
    {
        do {
            errno = 0;
            dp    = readdir(dirp);
            if (dp != nullptr)
            {
                if(strcmp(dp->d_name, ".") != 0 && strcmp(dp->d_name, "..") != 0)
                {
                    if (dp->d_type == DT_DIR)
                    {
                        std::string str(path);
                        str += _SEPARATOR_;
                        str += dp->d_name;
                        listFilesFromFolder(str.c_str(),fileList);
                    }
                    else if(dp->d_type == DT_REG )
                    {
                        std::string str(path);
                        str += _SEPARATOR_;
                        str += dp->d_name;
                        fileList.emplace_back(str);
                    }
                }
            }
        }while (dp != nullptr);
        closedir(dirp);
    }
    else
    {
        const char *  pSError = strerror(errno);
        ERROR_LOG("Could not open the folder [%s]\n[%s]",path, pSError ? pSError : "");
    }
}

const char * createRandomPath(sqlite3_context *context)
{
    #if defined _WIN32
    char lpBuffer[255] = "";
    char *dir_name = nullptr;
    
    auto fRetError = [] (sqlite3_context *context) -> const char * 
    {
        const DWORD lerr = GetLastError();
        if(lerr)
        {
            char *message = nullptr;
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, nullptr, lerr, 0, (char *)&message, 0,nullptr);
            ERROR_LOG("GetTempPath failed [%s]",message ? message : "");
            sqlite3_result_error(context, "GetTempPath failed", -1);
            LocalFree(message);
        }
        else
        {
            sqlite3_result_error(context, "error on GetTempPath", -1);
        }
        return "";
    };
    std::string strdir_path;
    if(GetTempPathA(sizeof(lpBuffer),lpBuffer) != 0 )
    {
        const int len = strlen(lpBuffer);
        if(len > 0 && lpBuffer[len-1] == '\\' )
            lpBuffer[len-1] = 0;
        strdir_path = lpBuffer;
        strdir_path += "\\asset_";
        for(unsigned int i=0; i < 6; ++i)
        {
            strdir_path.push_back(util::getRandomChar('A','Z'));
        }
        dir_name = const_cast<char*>(strdir_path.c_str());
        if( _mkdir( dir_name ) != 0 )
        {
            return fRetError(context);
        }
    }
    else
    {
        return fRetError(context);
    }
    #else

    #if defined          ANDROID
        util::COMMON_JNI *cJni        = util::COMMON_JNI::getInstance();
        const char *     currentPath  = cJni->absPath.c_str();
        char template_name[255]       = "";
        snprintf(template_name,sizeof(template_name),"%s/asset_XXXXXX",currentPath);
        char *dir_name = mkdtemp(template_name);
    #else
        char template_name[] = "/tmp/asset_XXXXXX";
        char *dir_name = mkdtemp(template_name);
    #endif
    if(dir_name == nullptr)
    {
        const char *  pSError = strerror(errno);
        ERROR_LOG("%s","mkdtemp returned NULL [%s]",pSError ? pSError : "");
        sqlite3_result_error(context, "mkdtemp returned NULL", -1);
        return "";
    }
    #endif
    addPathToEngine(dir_name);
    return paths[paths.size()-1].c_str();
}

const char * getLastPath(sqlite3_context *context)
{
    if(paths.size())
        return paths[paths.size()-1].c_str();
    return createRandomPath(context);
}

const bool exitPathInVector(const std::vector<std::string> &_paths, const char * folder)
{
    for(unsigned int i=0; i < _paths.size(); ++i)
    {
        if(sqlite3_stricmp(_paths[i].c_str(),folder) == 0 )
        {
            return true;
        }
    }
    return false;
}

void selectAllPathFromDumpedFolderTable(sqlite3_context *context,std::vector<std::string> &_paths)
{
    sqlite3_stmt *pStmt = nullptr;
    sqlite3 * db        = sqlite3_context_db_handle(context);
    char * sql          = sqlite3_mprintf("SELECT DISTINCT path FROM dumped_folder;");
    int rc              = sqlite3_prepare_v2(db, sql, -1, &pStmt, 0);
    if(rc == SQLITE_OK)
    {
        while(sqlite3_step(pStmt) == SQLITE_ROW)
        {
            const char* folder = reinterpret_cast<const char*>(sqlite3_column_text(pStmt, 0));
            if(exitPathInVector(_paths, folder) == false)
            {
                addPathToEngine(folder);
            }
        }
        sqlite3_finalize(pStmt);
    }
    else
    {
        const char * err_msg = sqlite3_errmsg(db);
        ERROR_LOG("%s",err_msg);
        sqlite3_free(sql);
        sqlite3_result_error(context, err_msg, -1);
    }
}

void deletePathFromDumpedFolderTable(sqlite3_context *context,const char * path)
{
    char *err_msg       = nullptr;
    sqlite3 * db        = sqlite3_context_db_handle(context);
    char * sql          = sqlite3_mprintf("DELETE FROM dumped_folder WHERE path = '%s';",path);
    int rc              = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if(rc != SQLITE_OK)
    {
        ERROR_LOG("%s",err_msg);
        sqlite3_free(sql);
        sqlite3_result_error(context, err_msg, -1);
        sqlite3_free(err_msg);
    }
}

static void deleteAssetFoldersFunc(sqlite3_context *context,int argc,sqlite3_value ** argv)
{
    if(argc > 0)
    {
        std::vector<std::string> paths_to_delete;
        bool argsOkay = true;
        for(int n = 0; n < argc; ++n)
        {
            if( sqlite3_value_type(argv[n]) != SQLITE_INTEGER )
            {
                std::string msg("Expected number referent to asset folder created <1 to number of assets>");
                ERROR_LOG("%s \nTotal number of folders:%d", msg.c_str(), paths.size());
                sqlite3_result_error(context, msg.c_str(), -1);
                argsOkay = false;
                break;
            }
            else
            {
                int nIndex    =  static_cast<const int>(sqlite3_value_int(argv[n]));
                if(nIndex < 0)
                    nIndex = paths.size() - (-nIndex) + 1;
                if(nIndex < 1)
                {
                    std::string msg("Invalid number referent to asset folder created <ONE based> <1 to number of assets>");
                    ERROR_LOG("%s \nTotal number of folders:%d", msg.c_str(), paths.size());
                    sqlite3_result_error(context, msg.c_str(), -1);
                    argsOkay = false;
                    break;
                }
                else if(nIndex > static_cast<int>(paths.size()))
                {
                    std::string msg("Invalid number referent to asset folder created <ONE based> <1 to number of assets>");
                    ERROR_LOG("%s \nTotal number of folders:%d", msg.c_str(), paths.size());
                    sqlite3_result_error(context, msg.c_str(), -1);
                    argsOkay = false;
                    break;
                }
                else
                {
                    const int index = nIndex - 1;
                    paths_to_delete.push_back(paths[index]);
                }
            }
        }
        if(argsOkay)
        {
            for (size_t i = 0; i < paths_to_delete.size(); i++)
            {
                const char * folder = paths_to_delete[i].c_str();
                std::vector<std::string> allFiles;
                listFilesFromFolder(folder, allFiles);
                for (size_t j = 0; j < allFiles.size(); j++)
                {
                    const char * fileName = allFiles[j].c_str();
                    if(remove(fileName) != 0)
                    {
                        const char *  pSError = strerror(errno);
                        ERROR_LOG("Could not remove the file [%s][%s]",fileName,pSError);
                    }
                }
                if(rmdir(folder) == -1)
                {
                    std::string msg("Error on remove folder [");
                    msg += paths_to_delete[i];
                    msg += "] ";
                    perror ("tempdir: error: ");
                    ERROR_LOG("%s",msg.c_str());
                    sqlite3_result_error(context, msg.c_str(), -1);
                    argsOkay = false;
                    break;
                }
                deletePathFromDumpedFolderTable(context,folder);
            }
            if(argsOkay)
            {
                for(int n = 0; n < argc; ++n)
                {
                    int nIndex    =  static_cast<const int>(sqlite3_value_int(argv[n]));
                    if(nIndex < 0)
                        nIndex = paths.size() - (-nIndex) + 1;
                    const int index     = nIndex - 1;
                    paths[index].clear();
                }
                std::vector<std::string> left_paths;
                for (size_t i = 0; i < paths.size(); i++)
                {
                    if(paths[i].length() > 0)
                        left_paths.push_back(paths[i]);
                }
                paths = std::move(left_paths);
            }
        }
    }
    else
    {
        selectAllPathFromDumpedFolderTable(context,paths);
        for (size_t i = 0; i < paths.size(); i++)
        {
            const char * folder = paths[i].c_str();
            std::vector<std::string> allFiles;
            listFilesFromFolder(folder, allFiles);
            for (size_t j = 0; j < allFiles.size(); j++)
            {
                const char * fileName = allFiles[j].c_str();
                if(remove(fileName) != 0)
                {
                    const char *  pSError = strerror(errno);
                    ERROR_LOG("Could not remove the file [%s][%s]",fileName,pSError);
                }
            }
            if(rmdir(folder) == -1)
            {
                std::string msg("Error on remove folder [");
                msg += paths[i];
                msg += "] ";
                perror ("tempdir: error: ");
                ERROR_LOG("%s",msg.c_str());
                sqlite3_result_error(context, msg.c_str(), -1);
            }
            deletePathFromDumpedFolderTable(context,folder);
        }
        paths.clear();
    }
}

bool existFolder(const char* sPath)
{
    struct stat sb;
    if (stat(sPath, &sb) == 0 && S_ISDIR(sb.st_mode)) 
        return true;
    return false;
}

bool hasSeparator(const char* sPath)
{
    std::string path(sPath);
    if(path.find('\\') != std::string::npos)
        return true;
    if(path.find('/') != std::string::npos)
        return true;
    return false;
}

static void addAssetFolderFunc(sqlite3_context *context,int argc,sqlite3_value **argv)
{
    if(argc > 0 && sqlite3_value_type(argv[0]) != SQLITE3_TEXT && sqlite3_value_type(argv[0]) != SQLITE_NULL )
    {
        ERROR_LOG("%s", "invalid argument. expected <TEXT> or <NULL>");
        sqlite3_result_error(context, "invalid argument. expected <TEXT> or <NULL>", -1);
    }
    else
    {
        auto fCreateFolder = [&context] (const char * sPath) -> void
        {
            #if defined _WIN32
            if (_mkdir(sPath) != 0)
            #else
            if (mkdir(sPath,0777) != 0)
            #endif
            {
                const char *  pSError = strerror(errno);
                ERROR_LOG("Failed to create folder [%s] [%s]",sPath,pSError ? pSError : "");
                sqlite3_result_error(context, "Failed to create folder", -1);
            }
            else
            {
                addPathToEngine(sPath);
            }
        };
        const char * sPath    =  argc > 0 ? reinterpret_cast<const char*>(sqlite3_value_text(argv[0])) : nullptr;
        if(sPath)
        {
            if(strcmp(sPath,".") == 0)
            {
            #if defined          ANDROID
                util::COMMON_JNI *cJni        = util::COMMON_JNI::getInstance();
                const char *     currentPath  = cJni->absPath.c_str();
                sPath                         = currentPath;
                cJni->addPathDroid(currentPath);
            #elif defined _WIN32
                char             dir[255]   = "";
                GetCurrentDirectoryA(sizeof(dir), dir);
                sPath                         = dir;
            #else
                char             dir[255]   = "";
                getcwd(dir,sizeof(dir));
                sPath                         = dir;
            #endif
                addPathToEngine(sPath);
            }
            else if (existFolder(sPath))
            {
                addPathToEngine(sPath);
            }
            else if (hasSeparator(sPath) == false)//is not full path
            {
                char dir_name[255]            = "";
                #if defined          ANDROID
                util::COMMON_JNI *cJni        = util::COMMON_JNI::getInstance();
                const char *     currentPath  = cJni->absPath.c_str();
                if(cJni->absPath.size() > 0 && cJni->absPath[cJni->absPath.size()-1] == '/')
                    snprintf(dir_name,sizeof(dir_name),"%s%s",currentPath,sPath);
                else
                    snprintf(dir_name,sizeof(dir_name),"%s/%s",currentPath,sPath);
                cJni->addPathDroid(dir_name);
                #elif defined _WIN32
                char lpBuffer[255]            = "";
                if(GetTempPathA(sizeof(lpBuffer),lpBuffer) != 0 )
                {
                    const int len = strlen(lpBuffer);
                    if(len > 0 && lpBuffer[len-1] == '\\' )
                        lpBuffer[len-1] = 0;
                    snprintf(dir_name,sizeof(dir_name),"%s\\%s",lpBuffer,sPath);
                }
                else
                {
                    snprintf(dir_name,sizeof(dir_name),"%s",sPath);
                }
                #else
                snprintf(dir_name,sizeof(dir_name),"/tmp/%s",sPath);
                #endif
                if (existFolder(dir_name) == false)
                {
                    fCreateFolder(dir_name);
                }
                else
                {
                    addPathToEngine(dir_name);
                }
            }
            else
            {
                if (existFolder(sPath) == false)
                {
                    fCreateFolder(sPath);
                }
                else
                {
                    addPathToEngine(sPath);
                }
            }
        }
        else
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
                
                char dir_name[255]            = "";
                char lpBuffer[255]            = "";
                if(GetTempPathA(sizeof(lpBuffer),lpBuffer) != 0 )
                {
                    const int len = strlen(lpBuffer);
                    if(len > 0 && lpBuffer[len-1] == '\\' )
                        lpBuffer[len-1] = 0;
                    snprintf(dir_name,sizeof(dir_name),"%s\\%s",lpBuffer,"sqlite_tmp");
                }
                else
                {
                    snprintf(dir_name,sizeof(dir_name),"%s","sqlite_tmp");
                }
                if (_mkdir(dir_name) != 0)
                {
                    ERROR_LOG("Failed to create folder:\n%sIgnoring for now...",dir_name);
                }
            #else
                char template_name[] = "/tmp/asset_XXXXXX";
                char *dir_name = mkdtemp(template_name);
            #endif
            if(dir_name)
            {
                addPathToEngine(dir_name);
            }
            else
            {
                ERROR_LOG("%s","mkdtemp returned NULL");
                sqlite3_result_error(context, "mkdtemp returned NULL", -1);
            }
        }
    }
}

static void saveAssetFunc(sqlite3_context *context,int /*argc*/,sqlite3_value **argv)
{
    if( sqlite3_value_type(argv[0]) != SQLITE3_TEXT )
    {
        ERROR_LOG("%s","invalid argument. expected <TEXT>");
        sqlite3_result_error(context, "invalid argument. expected <TEXT>", -1);
    }
    else if(sqlite3_value_type(argv[1]) != SQLITE_BLOB )
    {
        ERROR_LOG("%s","invalid argument. expected <BLOB>");
        sqlite3_result_error(context, "invalid argument. expected <BLOB>", -1);
    }
    else
    {
        const char * sName    =  reinterpret_cast<const char*>(sqlite3_value_text(argv[0]));
        const void * sContent =  static_cast<const void*>(sqlite3_value_blob(argv[1]));
        if(sName)
        {
            const  size_t sizeOfcontent = static_cast<size_t>(sqlite3_value_bytes(argv[1]));
            std::string path  = getLastPath(context);
            path.push_back(_SEPARATOR_);
            path += sName;
            FILE * file = fopen(path.c_str(),"wb");
            if (file)
            {
                const  size_t s = fwrite(sContent, 1, sizeOfcontent, file);
                if (s != sizeOfcontent)
                {
                    std::string msg("Error on write to file [");
                    msg += path;
                    msg += "] ";
                    ERROR_LOG("%s",msg.c_str());
                    sqlite3_result_error(context, msg.c_str(), -1);
                }
                fclose(file);
            }
            else
            {
                std::string msg("Could not open file [");
                msg += path;
                msg += "] to write";
                ERROR_LOG("%s",msg.c_str());
                sqlite3_result_error(context, msg.c_str(), -1);
            }
        }
        else
        {
            ERROR_LOG("%s","invalid argument. <TEXT> cannot be NULL");
            sqlite3_result_error(context, "invalid argument. <TEXT> cannot be NULL", -1);
        }
    }
}

bool existPathOnDumpedFolderTable(sqlite3_context *context,const char * path)
{
    bool exist          = false;
    sqlite3_stmt *pStmt = nullptr;
    sqlite3 * db        = sqlite3_context_db_handle(context);
    char * sql          = sqlite3_mprintf("SELECT path FROM dumped_folder WHERE path = \"%s\"",path);
    int rc              = sqlite3_prepare_v2(db, sql, -1, &pStmt, 0);
    if(rc == SQLITE_OK)
    {
        rc = sqlite3_step(pStmt);
        if (rc == SQLITE_ROW) 
        {
            exist = true;
        }
        sqlite3_finalize(pStmt);
    }
    else
    {
        const char * err_msg = sqlite3_errmsg(db);
        ERROR_LOG("%s",err_msg);
        sqlite3_free(sql);
        sqlite3_result_error(context, err_msg, -1);
    }
    return exist;
}

void insertPathToDumpedFolderTable(sqlite3_context *context,const char * path)
{
    char *err_msg       = nullptr;
    sqlite3 * db        = sqlite3_context_db_handle(context);
    char * sql          = sqlite3_mprintf("INSERT INTO dumped_folder(path) VALUES ('%s');",path);
    int rc              = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if(rc != SQLITE_OK)
    {
        ERROR_LOG("%s",err_msg);
        sqlite3_free(sql);
        sqlite3_result_error(context, err_msg, -1);
        sqlite3_free(err_msg);
    }
}

void createPathToDumpedFolderTable(sqlite3_context *context)
{
    char *err_msg       = nullptr;
    sqlite3 * db        = sqlite3_context_db_handle(context);
    const char * sql    = "CREATE TABLE IF NOT EXISTS dumped_folder(path TEXT);";
    int rc              = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if(rc != SQLITE_OK)
    {
        ERROR_LOG("%s",err_msg);
        sqlite3_result_error(context, err_msg, -1);
        sqlite3_free(err_msg);
    }
}

static void savePathAssetsFoldersFunc(sqlite3_context *context,int /*argc*/,sqlite3_value ** /*argv*/)
{
    if(paths.size() > 0)
    {
        createPathToDumpedFolderTable(context);
        for (size_t i = 0; i < paths.size(); i++)
        {
            const char* path = paths[i].c_str();
            if(existPathOnDumpedFolderTable(context,path) == false)
            {
                insertPathToDumpedFolderTable(context,path);
            }
        }
    }
}

extern "C"
{
    int sqlite3_assetpkg_init(sqlite3 *db,char ** /*pzErrMsg*/,const sqlite3_api_routines *pApi)
    {
        int rc = sqlite3_create_function(db,    //The first parameter is the database connection to which the SQL function is to be added.
                                    "SAVE_ASSET",//The second parameter is the name of the SQL function to be created or redefined.
                                    2,          //The third parameter (nArg) is the number of arguments that the SQL function or aggregate takes. 
                                                // the limit set by sqlite3_limit(SQLITE_LIMIT_FUNCTION_ARG). 
                                                // Current (6).If the parameter is less than -1 or greater than 127 then the behavior is undefined.
                                    SQLITE_UTF8|SQLITE_DIRECTONLY, //The fourth parameter, eTextRep, specifies what text encoding
                                    nullptr,//The fifth parameter is an arbitrary pointer. The implementation of the function can gain access to this pointer using sqlite3_user_data().
                                    //The sixth, seventh and eighth parameters passed to the three "sqlite3_create_function*" functions, 
                                    //xFunc, xStep and xFinal, are pointers to C-language functions that implement the SQL function or aggregate. 
                                    // A scalar SQL function requires an implementation of the xFunc callback only; 
                                    // NULL pointers must be passed as the xStep and xFinal parameters. 
                                    // An aggregate SQL function requires an implementation of xStep and xFinal and NULL pointer must be 
                                    // passed for xFunc. To delete an existing SQL function or aggregate, pass NULL pointers for all three 
                                    // function callbacks.
                                    saveAssetFunc, 
                                    nullptr, 
                                    nullptr);
        if(rc == SQLITE_OK)
        {
            rc = sqlite3_create_function(db,"ADD_ASSET_FOLDER",-1,SQLITE_UTF8|SQLITE_DIRECTONLY,nullptr,addAssetFolderFunc,nullptr,nullptr);
        }

        if(rc == SQLITE_OK)
        {
            rc = sqlite3_create_function(db,"DELETE_ASSET_FOLDER",-1,SQLITE_UTF8|SQLITE_DIRECTONLY,nullptr,deleteAssetFoldersFunc,nullptr,nullptr);
        }

        if(rc == SQLITE_OK)
        {
            rc = sqlite3_create_function(db,"SAVE_PATH_ASSETS",0,SQLITE_UTF8|SQLITE_DIRECTONLY,nullptr,savePathAssetsFoldersFunc,nullptr,nullptr);
        }


        return rc;
    }
}
