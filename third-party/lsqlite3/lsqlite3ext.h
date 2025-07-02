 #ifndef L_SQLITE3EXT_H
 #define L_SQLITE3EXT_H
 
 #include "sqlite3.h"
 
 extern int sqlite3_fileio_init(sqlite3 *db,char **pzErrMsg,const sqlite3_api_routines *pApi);
 extern int sqlite3_assetpkg_init(sqlite3 *db,char **pzErrMsg,const sqlite3_api_routines *pApi);

 const char* load_embedded_extension(sqlite3 *db,int * result) // return null when NO error occurred
 {
     *result = sqlite3_fileio_init(db,0,0);
     if(*result != SQLITE_OK)
        return sqlite3_errstr(*result);

    *result = sqlite3_assetpkg_init(db,0,0);
     if(*result != SQLITE_OK)
        return sqlite3_errstr(*result);

     //add any other function extension here
     return NULL;
 }

 #endif