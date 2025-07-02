/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef SHADER_CFG_H
#define SHADER_CFG_H

#include "core-exports.h"
#include <vector>
#include <map>
#include <string>

namespace mbm
{
    enum TYPE_VAR_SHADER : char;

    struct VAR_CFG
    {
        std::string                 name;
        TYPE_VAR_SHADER             type;
        float                       Min[4];
        float                       Max[4];
        float                       Default[4];
        float                       current[4];
        bool                        validVar;
        API_IMPL VAR_CFG(const TYPE_VAR_SHADER newType, const char *Name, const char *values, char *result,const std::size_t sizeResult);
    };

    class SHADER_CFG 
    {
      public:
        std::string             fileName;
        std::string             codeShader;
        std::vector<VAR_CFG *>  lsVar;
        API_IMPL SHADER_CFG(const char *FileName);
        API_IMPL virtual ~SHADER_CFG();
        API_IMPL void addVar(const char *type, const char *name, const char *values);
        API_IMPL VAR_CFG *getVarByName(const char *name);
        API_IMPL VAR_CFG *getVarFloat(const uint32_t indexTck);
        API_IMPL VAR_CFG *getVarColor(const uint32_t indexTck);
        API_IMPL VAR_CFG *getVarVector(const uint32_t indexTck);
    };

    class CFG_FROM_MEMORY
    {
    public:
        API_IMPL CFG_FROM_MEMORY();
        API_IMPL virtual ~CFG_FROM_MEMORY();
        API_IMPL bool parserCFGFromMemory(const char *cfgFromResource, const char comment = '#');
        API_IMPL const char *getValue(const char *key, const char *defaultValue) const;
        API_IMPL int getValue(const char *key, const int defaultValue) const;
        API_IMPL float getValue(const char *key, const float defaultValue) const;
        API_IMPL double getValue(const char *key, const double defaultValue) const;
        API_IMPL void clearContents() noexcept;
    private:
        void removeComment(std::string &line, const char comment) const;
        bool onlyWhitespace(const std::string &line) const;
        bool parseLine(const std::string &line, const uint32_t numLine);
        bool validLine(const std::string &line) const;
        void extractContents(const std::string &line);
        bool keyExists(const std::string &key) const;
        void extractKey(std::string &key, size_t const &sepPos, const std::string &line) const;
        void extractValue(std::string &value, size_t const &sepPos, const std::string &line) const;
    protected:
        std::map<std::string, std::string> contents;        
    };

    class SHADER_CFG_LOADER : public CFG_FROM_MEMORY
    {
      public:
        std::vector<SHADER_CFG *> lsPs;
        std::vector<SHADER_CFG *> lsVs;
    //#ifndef ANDROID
    //    std::string vertexPath;
    //    std::string pixelPath;
    //#endif
        //std::string mainPS;
        //std::string mainVS;
        //std::string versionPS;
        //std::string versionVS;
        API_IMPL SHADER_CFG_LOADER() noexcept;
        API_IMPL virtual ~SHADER_CFG_LOADER();
        API_IMPL bool parserCFGFromResource();
        API_IMPL void sortShader();
        //void createCFG(const char *fileName);
        API_IMPL SHADER_CFG *getShader(const char *fileName);
        API_IMPL void consolidatesFileShaderCFG();
        API_IMPL bool readShaderFromFile(const char *fileName, std::string &code);
      private:
        void addVariablesFromContents();
        void parserShaders();
        char *trimRight(char *stringSource);
        char *trimLeft(char *stringSource);
        char *trim(char *stringSource);
    };
}

#endif
