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

#include <shader-cfg.h>
#include <shader-resource.h>
#include <shader-var-cfg.h>
#include <util-interface.h>

#include <fstream>
#include <algorithm>
#include <cstring>


namespace util 
{
    extern void split(std::vector<std::string> &result, const char *in, const char delim);
    extern const char * getFullPath(const char *fileName, bool *existsFile );
};

enum TYPE_MMD
{
    NONE_MMD_TYPE,
    MIN_TYPE,
    MAX_TYPE,
    DEFAULT_TYPE
};

namespace mbm
{
    VAR_CFG::VAR_CFG(const TYPE_VAR_SHADER newType, const char *Name, const char *values, char *result,const std::size_t sizeResult):name(Name)
    {
        this->validVar = true;
        memset(this->Min, 0, sizeof(Min));
        memset(this->Max, 0, sizeof(Max));
        memset(this->Default, 0, sizeof(Default));
        memset(this->current, 0, sizeof(current));
        this->type = newType;
        std::vector<std::string> bySpace;
        util::split(bySpace, values, ' ');

        for (uint32_t i = 0; i < bySpace.size(); ++i)
        {
            if (bySpace[i].empty() || bySpace[i].compare("\t") == 0)
            {
                bySpace.erase(bySpace.begin() + i);
                i = 0xffffffff;
            }
        }

        if (this->type == VAR_FLOAT)
        {
            if (bySpace.size() != 6)
            {
                if (result && sizeResult > 0)
                    snprintf(result,sizeResult, "variable %s invalid. expected size: %d . size found:%ld", Name, 6,
                            static_cast<long>(bySpace.size()));
#if defined _DEBUG
                else
                    PRINT_IF_DEBUG( "variable %s invalid. expected size: %d . size found:%ld", Name, 6,
                                 static_cast<long>(bySpace.size()));
#endif
                this->validVar = false;
            }
        }
        else if (this->type == VAR_COLOR_RGBA)
        {
            if (bySpace.size() != 15)
            {
				if (result && sizeResult > 0)
                    snprintf(result,sizeResult, "variable %s invalid. expected size: %d . size found:%ld", Name, 15,
                            static_cast<long>(bySpace.size()));
#if defined _DEBUG
                else
                    PRINT_IF_DEBUG( "variable %s invalid. expected size: %d . size found:%ld", Name, 15,
                                 static_cast<long>(bySpace.size()));
#endif
                this->validVar = false;
            }
        }
        else if (this->type == VAR_VECTOR || this->type == VAR_COLOR_RGB)
        {
            if (bySpace.size() != 12)
            {
				if (result && sizeResult > 0)
                    snprintf(result,sizeResult, "variable %s invalid. expected size: %d . size found:%ld", Name, 12,
                            static_cast<long>(bySpace.size()));
#if defined _DEBUG
                else
                    PRINT_IF_DEBUG( "variable %s invalid. expected size: %d . size found:%ld", Name, 12,
                                 static_cast<long>(bySpace.size()));
#endif
                this->validVar = false;
            }
        }
        else if (this->type == VAR_VECTOR2)
        {
            if (bySpace.size() != 9)
            {
				if (result && sizeResult > 0)
					snprintf(result,sizeResult, "variable %s invalid. expected size: %d . size found:%ld", Name, 9,
                            static_cast<long>(bySpace.size()));
#if defined _DEBUG
                else
                    PRINT_IF_DEBUG( "variable %s invalid. expected size: %d . size found:%ld", Name, 9,
                                 static_cast<long>(bySpace.size()));
#endif
                this->validVar = false;
            }
        }
        else
        {
			if (result && sizeResult > 0)
				snprintf(result,sizeResult, "couldn't just happen");
#if defined _DEBUG
            else
                PRINT_IF_DEBUG( "couldn't just happen");
#endif
            this->validVar = false;
        }
        if (this->validVar)
        {
            std::vector<float> lsMin;
            std::vector<float> lsMax;
            std::vector<float> lsDefault;
            TYPE_MMD           lastType = NONE_MMD_TYPE;

            for (auto & i : bySpace)
            {
                const char *strArg = i.c_str();
                if (strcmp("min", strArg) == 0)
                {
                    lastType = MIN_TYPE;
                }
                else if (strcmp("max", strArg) == 0)
                {
                    lastType = MAX_TYPE;
                }
                else if (strcmp("default", strArg) == 0)
                {
                    lastType = DEFAULT_TYPE;
                }
                else if (lastType != NONE_MMD_TYPE)
                {
                    constexpr float prop = 1.0f / 255.0f;
                    switch (lastType)
                    {
                        case MIN_TYPE:
                        {
                            if (newType == VAR_COLOR_RGB || newType == VAR_COLOR_RGBA)
                            {
                                bool hasPoint = i.find('.') != std::string::npos;
                                if (hasPoint)
                                {
                                    lsMin.push_back(static_cast<float>(atof(strArg)));
                                }
                                else
                                {
                                    auto UCr     = static_cast<uint8_t>(std::atoi(strArg));
                                    const float  color    = prop * static_cast<float>(UCr);
                                    lsMin.push_back(color);
                                }
                            }
                            else
                            {
                                lsMin.push_back(static_cast<float>(atof(strArg)));
                            }
                        }
                        break;
                        case MAX_TYPE:
                        {
                            if (newType == VAR_COLOR_RGB || newType == VAR_COLOR_RGBA)
                            {
                                bool hasPoint = i.find('.') != std::string::npos;
                                if (hasPoint)
                                {
                                    lsMax.push_back(static_cast<float>(atof(strArg)));
                                }
                                else
                                {
                                    auto UCr     = static_cast<uint8_t>(std::atoi(strArg));
                                    const float  color    = prop * static_cast<float>(UCr);
                                    lsMax.push_back(color);
                                }
                            }
                            else
                            {
                                lsMax.push_back(static_cast<float>(atof(strArg)));
                            }
                        }
                        break;
                        case DEFAULT_TYPE:
                        {
                            if (newType == VAR_COLOR_RGB || newType == VAR_COLOR_RGBA)
                            {
                                bool hasPoint = i.find('.') != std::string::npos;
                                if (hasPoint)
                                {
                                    lsDefault.push_back(static_cast<float>(atof(strArg)));
                                }
                                else
                                {
                                    auto UCr     = static_cast<uint8_t>(std::atoi(strArg));
                                    const float  color    = prop * static_cast<float>(UCr);
                                    lsDefault.push_back(color);
                                }
                            }
                            else
                            {
                                lsDefault.push_back(static_cast<float>(atof(strArg)));
                            }
                        }
                        break;
                        default: {
                        }
                        break;
                    }
                }
            }
            if (this->type == VAR_FLOAT)
            {
                if (lsMin.size() != 1)
                {
					if (result && sizeResult > 0)
						snprintf(result,sizeResult, "variable %s invalid. argument expected 'min': %d . cfg:%ld", Name, 1,
                                static_cast<long>(lsMin.size()));
#if defined _DEBUG
                    else
                        PRINT_IF_DEBUG( "variable %s invalid. argument expected 'min': %d . cfg:%ld",
                                     Name, 1, static_cast<long>(lsMin.size()));
#endif
                    this->validVar = false;
                }
                if (lsMax.size() != 1)
                {
					if (result && sizeResult > 0)
						snprintf(result,sizeResult, "variable %s invalid. argument expected 'max': %d . cfg:%ld", Name, 1,
                                static_cast<long>(lsMax.size()));
#if defined _DEBUG
                    else
                        PRINT_IF_DEBUG( "variable %s invalid. argument expected 'max': %d . cfg:%ld",
                                     Name, 1, static_cast<long>(lsMax.size()));
#endif
                    this->validVar = false;
                }
                if (lsDefault.size() != 1)
                {
					if (result && sizeResult > 0)
						snprintf(result,sizeResult, "variable %s invalid. argument expected 'default': %d . cfg:%ld", Name, 1,
                                static_cast<long>(lsDefault.size()));
#if defined _DEBUG
                    else
                        PRINT_IF_DEBUG( "variable %s invalid. argument expected 'default': %d . cfg:%ld",
                                     Name, 1, static_cast<long>(lsDefault.size()));
#endif
                    this->validVar = false;
                }
            }
            else if (newType == VAR_COLOR_RGBA)
            {
                if (lsMin.size() != 4)
                {
					if (result && sizeResult > 0)
						snprintf(result,sizeResult, "variable %s invalid. argument expected 'min': %d . cfg:%ld", Name, 4,
                                static_cast<long>(lsMin.size()));
#if defined _DEBUG
                    else
                        PRINT_IF_DEBUG( "variable %s invalid. argument expected 'min': %d . cfg:%ld",
                                     Name, 4, static_cast<long>(lsMin.size()));
#endif
                    this->validVar = false;
                }
                if (lsMax.size() != 4)
                {
					if (result && sizeResult > 0)
						snprintf(result,sizeResult, "variable %s invalid. argument expected 'max': %d . cfg:%ld", Name, 4,
                                static_cast<long>(lsMax.size()));
#if defined _DEBUG
                    else
                        PRINT_IF_DEBUG( "variable %s invalid. argument expected 'max': %d . cfg:%ld",
                                     Name, 4, static_cast<long>(lsMax.size()));
#endif
                    this->validVar = false;
                }
                if (lsDefault.size() != 4)
                {
#if defined _DEBUG
                    PRINT_IF_DEBUG( "variable %s invalid. argument expected 'default': %d . cfg:%ld",
                                 Name, 4, static_cast<long>(lsDefault.size()));
#endif
                    this->validVar = false;
                }
            }
            else if (newType == VAR_COLOR_RGB || newType == VAR_VECTOR)
            {
                if (lsMin.size() != 3)
                {
					if (result && sizeResult > 0)
						snprintf(result,sizeResult, "variable %s invalid. argument expected 'min': %d . cfg:%ld", Name, 3,
                                static_cast<long>(lsMin.size()));
#if defined _DEBUG
                    else
                        PRINT_IF_DEBUG( "variable %s invalid. argument expected 'min': %d . cfg:%ld",
                                     Name, 3, static_cast<long>(lsMin.size()));
#endif
                    this->validVar = false;
                }
                if (lsMax.size() != 3)
                {
					if (result && sizeResult > 0)
						snprintf(result,sizeResult, "variable %s invalid. argument expected 'max': %d . cfg:%ld", Name, 3,
                                static_cast<long>(lsMax.size()));
#if defined _DEBUG
                    else
                        PRINT_IF_DEBUG( "variable %s invalid. argument expected 'max': %d . cfg:%ld",
                                     Name, 3, static_cast<long>(lsMax.size()));
#endif
                    this->validVar = false;
                }
                if (lsDefault.size() != 3)
                {
					if (result && sizeResult > 0)
						snprintf(result,sizeResult, "variable %s invalid. argument expected 'default': %d . cfg:%ld", Name, 3,
                                static_cast<long>(lsDefault.size()));
#if defined _DEBUG
                    else
                        PRINT_IF_DEBUG( "variable %s invalid. argument expected 'default': %d . cfg:%ld",
                                     Name, 3, static_cast<long>(lsDefault.size()));
#endif
                    this->validVar = false;
                }
            }
            else if (newType == VAR_VECTOR2)
            {
                if (lsMin.size() != 2)
                {
					if (result && sizeResult > 0)
						snprintf(result,sizeResult, "variable %s invalid. argument expected 'min': %d . cfg:%ld", Name, 2,
                                static_cast<long>(lsMin.size()));
#if defined _DEBUG
                    else
                        PRINT_IF_DEBUG( "variable %s invalid. argument expected 'min': %d . cfg:%ld",
                                     Name, 2, static_cast<long>(lsMin.size()));
#endif
                    this->validVar = false;
                }
                if (lsMax.size() != 2)
                {
					if (result && sizeResult > 0)
						snprintf(result,sizeResult, "variable %s invalid. argument expected 'max': %d . cfg:%ld", Name, 2,
                                static_cast<long>(lsMax.size()));
#if defined _DEBUG
                    else
                        PRINT_IF_DEBUG( "variable %s invalid. argument expected 'max': %d . cfg:%ld",
                                     Name, 2, static_cast<long>(lsMax.size()));
#endif
                    this->validVar = false;
                }
                if (lsDefault.size() != 2)
                {
					if (result && sizeResult > 0)
						snprintf(result,sizeResult, "variable %s invalid. argument expected 'default': %d . cfg:%ld", Name, 2,
                                static_cast<long>(lsDefault.size()));
#if defined _DEBUG
                    else
                        PRINT_IF_DEBUG( "variable %s invalid. argument expected 'default': %d . cfg:%ld",
                                     Name, 2, static_cast<long>(lsDefault.size()));
#endif
                    this->validVar = false;
                }
            }
            else
            {
                if (result && sizeResult > 0)
					snprintf(result,sizeResult, "couldn't just happen");
#if defined _DEBUG
                else
                    PRINT_IF_DEBUG( "couldn't just happen");
#endif
                this->validVar = false;
            }
            if (this->validVar)
            {
                for (uint32_t i = 0; i < lsMin.size(); ++i)
                {
                    this->Min[i]     = lsMin[i];
                    this->Max[i]     = lsMax[i];
                    this->Default[i] = lsDefault[i];
                    this->current[i] = lsDefault[i];
                }
            }
        }
    }

    SHADER_CFG::SHADER_CFG(const char *FileName):fileName(FileName)
    {
    }

    SHADER_CFG::~SHADER_CFG()
    {
        for (auto var : this->lsVar)
        {
            delete var;
        }
        this->lsVar.clear();
    }

    void SHADER_CFG::addVar(const char *type, const char *name, const char *values)
    {
        VAR_CFG *var = nullptr;
        for (uint32_t i = 0; i < this->lsVar.size(); ++i)
        {
            VAR_CFG *tVar = this->lsVar[i];
            if (strcmp(tVar->name.c_str(), name) == 0)
            {
                delete tVar;
                this->lsVar.erase(this->lsVar.begin() + i);
                break;
            }
        }
        if (strcmp("float", type) == 0)
        {
            var = new VAR_CFG(VAR_FLOAT, name, values,nullptr,0);
        }
        else if (strcmp("color", type) == 0 || strcmp("cor", type) == 0 || strcmp("rgb", type) == 0)
        {
            var = new VAR_CFG(VAR_COLOR_RGB, name, values,nullptr,0);
        }
        else if (strcmp("color-rgba", type) == 0 || strcmp("rgba", type) == 0)
        {
            var = new VAR_CFG(VAR_COLOR_RGBA, name, values,nullptr,0);
        }
        else if (strcmp("vector3", type) == 0 || strcmp("vector", type) == 0 || strcmp("vetor", type) == 0 || strcmp("xyz", type) == 0)
        {
            var = new VAR_CFG(VAR_VECTOR, name, values,nullptr,0);
        }
        else if (strcmp("vector2", type) == 0 || strcmp("vetor2", type) == 0 || strcmp("xy", type) == 0)
        {
            var = new VAR_CFG(VAR_VECTOR2, name, values,nullptr,0);
        }
        else
        {
#if defined _DEBUG
            PRINT_IF_DEBUG( "\nfailed: addVar. type of varible unknown: %s", type);
#endif
        }
        if (var)
        {
            if (var->validVar)
            {
                this->lsVar.push_back(var);
            }
            else
            {
#if defined _DEBUG
                PRINT_IF_DEBUG( "\nfailed to add variable [%s] to shader %s Linha %d", name,
                             this->fileName.c_str());
#endif
                delete var;
            }
        }
    }

    VAR_CFG * SHADER_CFG::getVarByName(const char *name)
    {
        for (auto var : this->lsVar)
        {
            if (strcmp(var->name.c_str(), name) == 0)
                return var;
        }
        return nullptr;
    }

    VAR_CFG * SHADER_CFG::getVarFloat(const uint32_t indexTck)
    {
        uint32_t currentIndexTck = 0;
        for (auto var : this->lsVar)
        {
            if (var->type == VAR_FLOAT)
            {
                if (indexTck == currentIndexTck)
                    return var;
                currentIndexTck++;
            }
        }
        return nullptr;
    }

    VAR_CFG * SHADER_CFG::getVarColor(const uint32_t indexTck)
    {
        uint32_t currentIndexTck = 0;
        for (auto var : this->lsVar)
        {
            if (var->type == VAR_COLOR_RGB || var->type == VAR_COLOR_RGBA)
            {
                if (indexTck == currentIndexTck)
                    return var;
                currentIndexTck++;
            }
        }
        return nullptr;
    }

    VAR_CFG * SHADER_CFG::getVarVector(const uint32_t indexTck)
    {
        uint32_t currentIndexTck = 0;
        for (auto var : this->lsVar)
        {
            if (var->type == VAR_VECTOR || var->type == VAR_VECTOR2)
            {
                if (indexTck == currentIndexTck)
                    return var;
                currentIndexTck++;
            }
        }
        return nullptr;
    }

    CFG_FROM_MEMORY::CFG_FROM_MEMORY()
    = default;

    CFG_FROM_MEMORY::~CFG_FROM_MEMORY() = default;

    bool CFG_FROM_MEMORY::parserCFGFromMemory(const char *cfgFromResource, const char comment)
    {
        std::vector<std::string> lsResult;
        util::split(lsResult, cfgFromResource, '\n');
        for (uint32_t i = 0; i < lsResult.size(); ++i)
        {
            std::string temp(lsResult[i]);
            if (temp.empty())
                continue;
            this->removeComment(temp, comment);
            if (temp.empty() || this->onlyWhitespace(temp))
                continue;
            if (!this->parseLine(temp, i + 1))
                return false;
        }
        return true;
    }

    bool CFG_FROM_MEMORY::parseLine(const std::string &line, const uint32_t numLine)
    {
        if (line.find('=') == line.npos)
        {
#if defined _DEBUG
            PRINT_IF_DEBUG( "\nCFG: not found '=' linha %d!!!\n", numLine);
#endif
            (void)numLine;
            return false;
        }
        if (!validLine(line))
        {
#if defined _DEBUG
            PRINT_IF_DEBUG( "\nCFG:invalid format line %d!!!\n", numLine);
#endif
            return false;
        }
        extractContents(line);
        return true;
    }

    bool CFG_FROM_MEMORY::onlyWhitespace(const std::string &line) const
    {
        return (line.find_first_not_of(' ') == line.npos);
    }

    void CFG_FROM_MEMORY::removeComment(std::string &line, const char comment) const
    {
        if (line.find(comment) != line.npos)
            line.erase(line.find(comment));
        auto s =  static_cast<uint32_t>(line.size());
        for (uint32_t i = 0; i < s; ++i)
        {
            if (line[i] == '\r')
                line[i] = ' ';
            if (line[i] == '\n')
                line[i] = ' ';
        }
    }

    bool CFG_FROM_MEMORY::validLine(const std::string &line) const
    {
        std::string temp = line;
        temp.erase(0, temp.find_first_not_of("\t "));
        if (temp[0] == '=')
            return false;

        for (size_t i = temp.find('=') + 1; i < temp.length(); ++i)
            if (temp[i] != ' ')
                return true;

        return false;
    }

    void CFG_FROM_MEMORY::extractContents(const std::string &line)
    {
        std::string temp = line;
        temp.erase(0, temp.find_first_not_of("\t "));
        size_t sepPos = temp.find('=');

        std::string key, value;
        extractKey(key, sepPos, temp);
        extractValue(value, sepPos, temp);

        if (!keyExists(key))
        {
            contents.insert(std::pair<std::string, std::string>(key, value));
        }
        else
        {
            contents[key] = value;
        }
    }

    void CFG_FROM_MEMORY::extractKey(std::string &key, size_t const &sepPos, const std::string &line) const
    {
        key = line.substr(0, sepPos);
        if (key.find('\t') != line.npos || key.find(' ') != line.npos)
            key.erase(key.find_first_of("\t "));
    }

    bool CFG_FROM_MEMORY::keyExists(const std::string &key) const
    {
        if (this->contents.size() == 0)
            return false;
        return contents.find(key) != contents.end();
    }

    void CFG_FROM_MEMORY::extractValue(std::string &value, size_t const &sepPos, const std::string &line) const
    {
        value = line.substr(sepPos + 1);
        value.erase(0, value.find_first_not_of("\t "));
        value.erase(value.find_last_not_of("\t ") + 1);
    }

    const char * CFG_FROM_MEMORY::getValue(const char *key, const char *defaultValue) const
    {
        if (!key)
            return defaultValue;
        std::string strKey(key);
        if (!keyExists(strKey))
            return defaultValue;
        const auto & it = contents.find(strKey);
        if(it != contents.cend())
            return it->second.c_str();
        return nullptr;
    }
    int CFG_FROM_MEMORY::getValue(const char *key, const int defaultValue) const
    {
        if (!key)
            return defaultValue;
        std::string strKey(key);
        if (!keyExists(strKey))
            return defaultValue;
        const auto & it = contents.find(strKey);
        if(it != contents.cend())
            return std::atoi(it->second.c_str());
        return 0;
    }
    float CFG_FROM_MEMORY::getValue(const char *key, const float defaultValue) const
    {
        if (!key)
            return defaultValue;
        std::string strKey(key);
        if (!keyExists(strKey))
            return defaultValue;
        const auto & it = contents.find(strKey);
        if(it != contents.cend())
            return static_cast<float>(atof(it->second.c_str()));
        return 0.0f;
    }
    double CFG_FROM_MEMORY::getValue(const char *key, const double defaultValue) const
    {
        if (!key)
            return defaultValue;
        std::string strKey(key);
        if (!keyExists(strKey))
            return defaultValue;
        const auto & it = contents.find(strKey);
        if(it != contents.cend())
            return static_cast<double>(atof(it->second.c_str()));
        return 0.0;
    }
    void CFG_FROM_MEMORY::clearContents() noexcept
    {
        this->contents.clear();
    }

    SHADER_CFG_LOADER::SHADER_CFG_LOADER() noexcept : CFG_FROM_MEMORY()
    {
    }

    SHADER_CFG_LOADER::~SHADER_CFG_LOADER()
    {
        for (auto ps : this->lsPs)
        {
            delete ps;
        }
        this->lsPs.clear();

        for (auto vs : this->lsVs)
        {
            delete vs;
        }
        this->lsVs.clear();
    }

    bool SHADER_CFG_LOADER::parserCFGFromResource()
    {
        for (const char **str = resourceShader; *str; str += 3)
        {
            const char *fileInResource = str[0];
            const char *code           = str[1];
            const char *cfgVariable    = str[2];
            size_t      len            = strlen(fileInResource);
            if (len > 3 && fileInResource[len - 1] == 's' && fileInResource[len - 2] == 'p')
            {
                auto varCfg = new SHADER_CFG(fileInResource);
                varCfg->codeShader = code;
                this->parserCFGFromMemory(cfgVariable);
                this->lsPs.push_back(varCfg);
            }
            else if (len > 3 && fileInResource[len - 1] == 's' && fileInResource[len - 2] == 'v')
            {
                auto varCfg = new SHADER_CFG(fileInResource);
                varCfg->codeShader = code;
                this->parserCFGFromMemory(cfgVariable);
                this->lsVs.push_back(varCfg);
            }
            else
            {
                PRINT_IF_DEBUG(
                             "resourceShader error! \ntype of file resource unknown (diferent from *.vs and *.ps). %s",
                             fileInResource);
                return false;
            }
        }
        this->addVariablesFromContents();
        this->parserShaders();
        this->clearContents();
        return true;
    }

    void SHADER_CFG_LOADER::sortShader()
    {
        std::sort(this->lsPs.begin(), this->lsPs.end(),
                  [](const SHADER_CFG *a, const SHADER_CFG *b) { return b->fileName.compare(a->fileName) > 0; });
        std::sort(this->lsVs.begin(), this->lsVs.end(),
                  [](const SHADER_CFG *a, const SHADER_CFG *b) { return b->fileName.compare(a->fileName) > 0; });
    }

    SHADER_CFG * SHADER_CFG_LOADER::getShader(const char *fileName)
    {
        if (fileName)
        {
            char f[255] = "";
            strncpy(f, fileName, sizeof(f)-1);
            if (strncmp("[ps-", f, 4) == 0 || strncmp("[vs-", f, 4) == 0) //[ps-color-keying.ps]
            {
                std::string tmp(&f[4]);
                size_t         len = tmp.size() - 1;
                strncpy(f, tmp.c_str(), len);
                f[len] = 0;
            }
            if (strncmp("ps-", f, 3) == 0 || strncmp("vs-", f, 3) == 0) // ps-color-keying.ps
            {
                std::string tmp(&f[3]);
                size_t         len = tmp.size();
                strncpy(f, tmp.c_str(), len);
                f[len] = 0;
            }
            for (int i = 0; i < 255 && f[i]; ++i)
            {
                if (f[i] == '-')
                    f[i] = ' ';
            }
            for (auto ps : this->lsPs)
            {
                if (strcmp(ps->fileName.c_str(), fileName) == 0 || strcmp(ps->fileName.c_str(), f) == 0)
                {
                    return ps;
                }
            }
            for (auto vs : this->lsVs)
            {
                if (strcmp(vs->fileName.c_str(), fileName) == 0 || strcmp(vs->fileName.c_str(), f) == 0)
                {
                    return vs;
                }
            }
        }
        return nullptr;
    }

    void SHADER_CFG_LOADER::addVariablesFromContents()
    {
        for (const auto & content : this->contents)
        {
            const char *key = content.first.c_str();
            size_t         len = strlen(key);
            if (len > 6)
            {
                std::vector<std::string> result;
                util::split(result, key, '[');
                if (result.size() >= 3)
                {
                    for (uint32_t i = 0; i < result.size(); ++i)
                    {
                        if (result[i].empty())
                        {
                            result.erase(result.begin() + i);
                            i = 0xffffffff;
                        }
                        else
                        {
                            if (result[i][result[i].size() - 1] == ']')
                                result[i].resize(result[i].size() - 1);
                        }
                    }
                    if (result.size() == 3)
                    {
                        SHADER_CFG *shaderCfg = this->getShader(result[0].c_str());
                        if (shaderCfg)
                        {
                            const char *values = this->getValue(key, "key search not founded");
                            if (values)
                                shaderCfg->addVar(result[1].c_str(), result[2].c_str(), values);
                            else
                                PRINT_IF_DEBUG( "\n  error on get key value:'%s values: %s'", key,
                                             values);
                        }
                        else
                        {
                            PRINT_IF_DEBUG( "\n shaderCfg '%s' not included in the SHADER_CFG_LOADER",
                                         result[0].c_str());
                        }
                    }
                    else
                    {
                        PRINT_IF_DEBUG( "\n result.size() != 3");
                    }
                }
            }
        }
    }

    void SHADER_CFG_LOADER::parserShaders()
    {
        // procura somente pelos arquivos primeiramente
        for (const auto & content : this->contents)
        {
            const char *key = content.first.c_str();
            size_t         len = strlen(key);
            if (len > 6)
            {
                if (key[0] == '[' && key[1] == 'p' && key[2] == 's' && key[len - 1] == ']' && key[len - 2] == 's' &&
                    key[len - 3] == 'p') //[ps && ps] arquivo Pixel shader
                {
                    std::vector<std::string> result;
                    util::split(result, key, '[');
                    if (result.size() == 1 || (result.size() == 2 && (result[0].empty() || result[1].empty())))
                    {
                        SHADER_CFG *varCfg = this->getShader(key);
                        if (varCfg == nullptr) // not in resource
                        {
                            varCfg = new SHADER_CFG(content.second.c_str());
                            this->lsPs.push_back(varCfg);
                        }
                    }
                }
                else if (key[0] == '[' && key[1] == 'v' && key[2] == 's' && key[len - 1] == ']' && key[len - 2] == 's' &&
                         key[len - 3] == 'v') //[vs && vs] arquivo vertex shader
                {
                    std::vector<std::string> result;
                    util::split(result, key, '[');
                    if (result.size() == 1 || (result.size() == 2 && (result[0].empty() || result[1].empty())))
                    {
                        SHADER_CFG *varCfg = this->getShader(key);
                        if (varCfg == nullptr) // not in resource
                        {
                            varCfg = new SHADER_CFG(content.second.c_str());
                            this->lsVs.push_back(varCfg);
                        }
                    }
                }
            }
        }
    }

    void SHADER_CFG_LOADER::consolidatesFileShaderCFG()
    {
        // add extra files from CFG
        for (const auto & content : this->contents)
        {
            const size_t s = content.first.size();
            if (s > 9) //[ps-____name_shader______.ps]
            {
                const std::string n = content.first;
                if (n.compare(0, 4, "[ps-") == 0 && n.compare(s - 4, 4, ".ps]") == 0) // is file PS
                {
                    auto varCfg = new SHADER_CFG(content.second.c_str());
                    if (this->readShaderFromFile(content.second.c_str(), varCfg->codeShader))
                        this->lsPs.push_back(varCfg);
                    else
                    {
                        PRINT_IF_DEBUG( "\n shaderCfg '%s' not found", varCfg->fileName.c_str());
                        delete varCfg;
                    }
                }
                if (n.compare(0, 4, "[vs-") == 0 && n.compare(s - 4, 4, ".vs]") == 0) // is file VS
                {
                    auto varCfg = new SHADER_CFG(content.second.c_str());
                    if (this->readShaderFromFile(content.second.c_str(), varCfg->codeShader))
                        this->lsVs.push_back(varCfg);
                    else
                    {
                        PRINT_IF_DEBUG( "\n shaderCfg '%s' not found", varCfg->fileName.c_str());
                        delete varCfg;
                    }
                }
            }
        }
    }

    bool SHADER_CFG_LOADER::readShaderFromFile(const char *fileName, std::string &code)
    {
        std::ifstream      fp(util::getFullPath(fileName,nullptr));
        std::string        line;
        if (!fp.is_open())
        {
            return false;
        }
        while (std::getline(fp, line))
        {
            code += line;
            code += "\n";
        }
        fp.close();
        return true;
    }

    char * SHADER_CFG_LOADER::trimRight(char *stringSource)
    {
        char *stringPtr   = stringSource + strlen(stringSource) - 1;
        while ((stringPtr >= stringSource) && (*stringPtr == ' '))
        {
            *stringPtr-- = 0;
        }
        return stringSource;
    }

    char * SHADER_CFG_LOADER::trimLeft(char *stringSource)
    {
        char *stringPtr   = stringSource;
        while (*stringPtr == ' ')
        {
            (void)*stringPtr++;
        }
        return stringPtr;
    }

    char * SHADER_CFG_LOADER::trim(char *stringSource)
    {
        return trimRight(trimLeft(stringSource));
    }

    /*void SHADER_CFG_LOADER::messageBox(const char *format, ...)
    {
        va_list va_args;
        va_start(va_args, format);
        size_t length = static_cast<size_t>(vsnprintf(nullptr, 0, format, va_args));
        va_end(va_args);
        va_start(va_args, format);
        char *_buffer = log_util::formatNewMessage(length, format, va_args);
        va_end(va_args);
#if defined(__MINGW32__) || defined(ANDROID) || defined(__linux__)
        PRINT_IF_DEBUG( _buffer);
#else
        MessageBoxA(nullptr, _buffer, "SHADER_CFG_LOADER", MB_OK);
#endif
        delete[] _buffer;
    }*/
}

