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

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include <lua-wrap/manager-lua.h>
#include <lua-wrap/timer-lua.h>
#include <lua-wrap/launcher-lua.h>
#include <core_mbm/device.h>
#include <core_mbm/dynamic-var.h>
#include <core_mbm/util-interface.h>
#include <core_mbm/gles-debug.h>
#include <core_mbm/renderizable-clone.h>
#include <version/version.h>
#include <static-resource/mini-mbm-logo.h>
#include <static-resource/hourglass-resource.h>
#include <render/texture-view.h>
#include <cstdlib>
#include <cstdlib>
#include <cctype>

#if defined ANDROID
    #include <platform/common-jni.h>
#endif

#ifdef _WIN32
    #pragma warning(push)
    #pragma warning(disable : 4702)
    #pragma warning(disable : 4127)
    #pragma warning(disable : 4505)
#endif

enum ARGS_LUA
{
    NONE,
    WIDTH_SCREEN,
    HEIGHT_SCREEN,
    EXPECTED_WIDTH_SCREEN,
    EXPECTED_HEIGHT_SCREEN,
    POSITION_X_SCREEN,
    POSITION_Y_SCREEN,
    MAXIMIZED_WINDOW,
    INITIAL_SCENE_LUA,
    NAME_APP,
    ADD_PATH,
    NO_SPLASH,
	NO_BORDER,
    ENABLE_RESIZE_WINDOW,
    EXECUTE_STRING
};

namespace mbm
{
    extern void onEndStreamCallBackFromSceneThread(lua_State *lua, USER_DATA_AUDIO_LUA *userData);
    extern void onEndStreamCallBack(const char *fileNameStream, USER_DATA_AUDIO_LUA *userData);
    extern void registerNamespaceMBM(lua_State *lua, SCENE *scene, lua_CFunction OnNewScene, lua_CFunction OnGetSplash);
	extern RENDERIZABLE *getRenderizableFromRawTable(lua_State *lua, const int rawi, const int indexTable);
	extern int newNoGCFromRenderizable(lua_State * lua,RENDERIZABLE * renderizable);
	extern void lua_print_line(lua_State *lua,TYPE_LOG type_log,const char *format, ...);

        LUA_MANAGER* LUA_MANAGER::pLuaManager = nullptr;

        SCENE_SCRIPT::SCENE_SCRIPT(const char *nameFileScriptLua, const bool _noSplash,RENDERIZABLE * previousSplash) :
            scriptLua(nameFileScriptLua), noSplash(_noSplash)
        {
            this->lua                   = nullptr;
			this->wasError              = false;
            this->textureLogo           = nullptr;
            this->textureRestore        = nullptr;
            this->loadSceneOnFirtLoop   = false;
            this->__onErrorStop__       = false;
            this->userData              = &this->dataScene;
            this->time_resize_window    = 0.0f;
			this->splashRenderizable  = mbm::clone(this,previousSplash);
        }
        
        SCENE_SCRIPT::~SCENE_SCRIPT()
        {
            this->device->removeObjectByIdSceneScene(this->getIdScene());
            if (this->lua)
                lua_close(this->lua);
            this->lua = nullptr;
            if (this->textureLogo)
                delete this->textureLogo;
            this->textureLogo = nullptr;
            if (this->textureRestore)
                delete this->textureRestore;
            this->textureRestore = nullptr;
			if(this->splashRenderizable)
				delete this->splashRenderizable;
			this->splashRenderizable = nullptr;
        }

		void SCENE_SCRIPT::setRenderizableLoading(RENDERIZABLE * renderizable)
		{
			if(this->splashRenderizable)
				delete this->splashRenderizable;
			this->splashRenderizable = nullptr;
			if(renderizable)
			{
				this->splashRenderizable = mbm::clone(this,renderizable);
			}
		}

		lua_State* SCENE_SCRIPT::getLuaState() const
		{
			return lua;
		}

		int SCENE_SCRIPT::OnGetSplash(lua_State* lua)
		{
			auto *luaManager = static_cast<LUA_MANAGER *>(LUA_MANAGER::pLuaManager);
			if (luaManager)
			{
				auto *curScene = static_cast<SCENE_SCRIPT*>(luaManager->device->scene);
				if(curScene && curScene->splashRenderizable)
				{
					auto *userData  = static_cast<USER_DATA_RENDER_LUA *>(curScene->splashRenderizable->userData);
					if(userData == nullptr)
					{
						return newNoGCFromRenderizable(lua,curScene->splashRenderizable);
					}
					else
					{
						userData->refTableLua(lua, 1, &userData->ref_MeAsTable);
						lua_rawgeti(lua, LUA_REGISTRYINDEX, userData->ref_MeAsTable);
						return 1;
					}
				}
			}
			lua_pushnil(lua);
			return 1;
		}

        void * SCENE_SCRIPT::get_lua_state()//if we are using lua we should be able to retrieve the current state
        {
            return this->lua;
        }

        void SCENE_SCRIPT::init() 
        {
            if (this->lua == nullptr)
            {
                if (this->createSceneLua() == false)
                {
                    this->device->run = false;
                    return;
                }
            }
            this->dataScene.lua = this->lua;

			if(LUA_MANAGER::pLuaManager)
            {
                auto expectedWidth  = static_cast<int>(device->getBackBufferWidth());
                auto expectedHeight = static_cast<int>(device->getBackBufferHeight());

                DYNAMIC_VAR * D_expectedW = this->device->lsDynamicVarGlobal["expectedwidth"];
                if(D_expectedW && D_expectedW->type == DYNAMIC_INT)
                    expectedWidth = static_cast<unsigned int>(D_expectedW->getInt());
                else if(D_expectedW && D_expectedW->type == DYNAMIC_FLOAT)
                    expectedWidth = static_cast<unsigned int>(D_expectedW->getFloat());

                DYNAMIC_VAR * D_expectedH = this->device->lsDynamicVarGlobal["expectedheight"];
                if(D_expectedH->type == DYNAMIC_INT)
                    expectedHeight = static_cast<unsigned int>(D_expectedH->getInt());
                else if(D_expectedH->type == DYNAMIC_FLOAT)
                    expectedHeight = static_cast<unsigned int>(D_expectedH->getFloat());
                
                char stretch[3] = "y";
                DYNAMIC_VAR * D_scaleTo = this->device->lsDynamicVarGlobal["stretch"];
                if(D_scaleTo && D_scaleTo->type == DYNAMIC_CSTRING)
                {
                    const char * pScale = D_scaleTo->getString();
                    strncpy(stretch,pScale,2);
                }
                this->device->scaleToScreen(static_cast<float>(expectedWidth),static_cast<float>(expectedHeight), stretch);
            }
            if (SCENE_SCRIPT::logo_was_init == false && this->noSplash == false)
            {
                this->device->colorClearBackGround = COLOR(1.0f,1.0f,1.0f,1.0f);
                this->device->camera.position2d.x  = 0;
                this->device->camera.position2d.y  = 0;
                bool        exitsFile              = false;
                this->textureLogo = new TEXTURE_VIEW(this, true, true);
                if (this->textureLogo && this->textureLogo->load(&resource_mini_mbm_logo))
                {
                    std::string restoreLua(util::getFullPath("restore.lua",&exitsFile));
                    const char *touchKeyExit = "\n"
                                               "keyCodeBack = 0 -- Back (Windows, linux -> ESC,   Android KEYCODE_BACK)\n"
                                               "if mbm.is(\"Android\") then\n"
                                               "    keyCodeBack = mbm.getKeyCode(\"KEYCODE_BACK\")\n"
                                               "else\n"
                                               "    keyCodeBack = mbm.getKeyCode(\"ESC\")\n"
                                               "end\n"
                                               "function onKeyDown(key)\n"
                                               "    if key == keyCodeBack then\n"
                                               "        mbm.quit()\n"
                                               "    end\n"
                                               "end\n"

                        ;
                    if (this->scriptLua.size() == 0)
                        this->scriptLua = "main.lua";
                    log_util::replaceString(this->scriptLua, "\\", "\\\\");
                    log_util::replaceString(restoreLua, "\\", "\\\\");
                    this->textureLogo->position.x = this->device->getScaleBackBufferWidth() / 2.0f;
                    this->textureLogo->position.y = this->device->getScaleBackBufferHeight() / 2.0f;
					this->textureLogo->position.z = -110.0f;
                    std::string luaFile           = "function onTimeOutChangeScene(ti) ";
                    luaFile += " mbm.loadScene(\"";
                    luaFile += exitsFile ? restoreLua : this->scriptLua;
                    luaFile += "\"); ";
                    luaFile += " ti:stop(); ";
                    luaFile += "end ";
                    luaFile += "t = timer:new(onTimeOutChangeScene,";
    #ifdef _DEBUG
                    luaFile += "0.1) ";
    #else

                    if (exitsFile)
                        luaFile += "0.3) ";
                    else
                        luaFile += "1.5) ";
    #endif
                    char tmpEndSceneScript[255] = "";
                    snprintf(tmpEndSceneScript,sizeof(tmpEndSceneScript) - 1,
											   "\nfunction onEndScene()\n"
                                               "    local camera2d = mbm.getCamera('2d')\n"
                                               "    camera2d:scaleToScreen(%d,%d,'y')\n"
                                               "end\n",
                            (int)this->device->getBackBufferWidth(), (int)this->device->getBackBufferHeight());

                    luaFile += touchKeyExit;
                    luaFile += tmpEndSceneScript;
                    this->device->disableAllButThis(this->textureLogo);
                    this->device->__swapBackBufferStep = 3;
                    
                    if (luaL_dostring(this->lua, luaFile.c_str()))
                    {
                        lua_print_line(lua,TYPE_LOG_ERROR,"\n%s", luaL_checkstring(lua, -1));
                        loadMainScene(this->device->ptrManager,
                                      this->scriptLua.size() > 0 ? this->scriptLua.c_str() : "main.lua");
                    }
                    
                }
                else
                {
                    loadMainScene(this->device->ptrManager,
                                  this->scriptLua.size() > 0 ? this->scriptLua.c_str() : "main.lua");
                }
                SCENE_SCRIPT::logo_was_init = true;
            }
            else if(static_cast<LUA_MANAGER *>(device->ptrManager)->execute_string(this->lua))
            {
                SCENE_SCRIPT::logo_was_init = true;
            }
			else
			{
				bool sucess                 = false;
				SCENE_SCRIPT::logo_was_init = true;
		#ifdef ANDROID
				const char *newPath = util::COMMON_JNI::getInstance()->copyFileFromAsset(this->scriptLua.c_str(), "rt");
			#if _DEBUG
                if(device->verbose)
				    INFO_LOG("new path [%s]", newPath ? newPath : "NULL");
			#endif
		#else
				const char *newPath = util::getFullPath(this->scriptLua.c_str(), nullptr);
		#endif
				if (newPath)
				{
					if (this->doLauncher(newPath))
					{
						this->fileNameScriptLuaFinal = newPath;
						sucess = true;
					}
					else if (!luaL_dofile(this->lua, newPath))
					{
						this->fileNameScriptLuaFinal = newPath;
						sucess                       = true;
					}
					else
					{
		#ifdef ANDROID
						if (this->doFileAsString(this->scriptLua.c_str()))
						{
							this->fileNameScriptLuaFinal = this->scriptLua;
							sucess                       = true;
						}
						else
		#endif
						{
							lua_print_line(lua,TYPE_LOG_ERROR,"\n%s", luaL_checkstring(lua, -1));
							this->wasError = true;
						}
					}
				}
				else
				{
					lua_print_line(lua,TYPE_LOG_ERROR,"error on open file %s!", this->scriptLua.c_str());
					this->scriptLua = "main.lua";
		#ifdef ANDROID
					newPath = util::COMMON_JNI::getInstance()->copyFileFromAsset(this->scriptLua.c_str(), "rt");
		  #if _DEBUG
					lua_print_line(lua,TYPE_LOG_INFO,"new path [%s]", newPath ? newPath : "NULL");
		  #endif
		#else
					newPath         = util::getFullPath(this->scriptLua.c_str(), nullptr);
		#endif
					if (newPath)
					{
						if(this->doLauncher(newPath))
						{
							this->fileNameScriptLuaFinal = newPath;
							sucess = true;
						}
						else if (!luaL_dofile(this->lua, newPath))
						{
							this->fileNameScriptLuaFinal = newPath;
							sucess                       = true;
						}
						else
						{
		#ifdef ANDROID
							if (this->doFileAsString(this->scriptLua.c_str()))
							{
								this->fileNameScriptLuaFinal = this->scriptLua;
								sucess                       = true;
							}
							else
		#endif
							{
								this->wasError = true;
								lua_print_line(lua,TYPE_LOG_ERROR,"\n%s", luaL_checkstring(lua, -1));
							}
						}
					}
					else
					{
						this->wasError = true;
						lua_print_line(lua,TYPE_LOG_ERROR,"error on open file %s!", "main.lua");
					}
				}
				if (sucess)
				{
                    lua_getglobal(this->lua, "onInitScene");
					if (lua_isfunction(this->lua, -1))
					{
						if (lua_pcall(this->lua, 0, 0, 0))
						{
							this->wasError = true;
							lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onInitScene", luaL_checkstring(lua, -1),this->scriptLua.c_str());
						}
						lua_settop(lua,0);
					}
					else
					{
						lua_pop(this->lua, 1);
					}
                    if (this->textureLogo)
				        delete this->textureLogo;
				    this->textureLogo = nullptr;
                    if (this->textureRestore)
                    {
                        delete this->textureRestore;
                        this->textureRestore = nullptr;
                    }
				}
				removePreviousSceneOnUnload();
			}
        }
        
        void SCENE_SCRIPT::logic()
        {
            if (this->wasError && __onErrorStop__)
                return;
            if (this->loadSceneOnFirtLoop)
            {
                this->loadSceneOnFirtLoop = false;
                std::string str("mbm.loadScene('");
                str += this->strNameSceneLoadOnFirtLoop;
				str += "')";
				if (luaL_dostring(this->lua, str.c_str()))
                {
                    this->wasError = true;
                    lua_print_line(lua,TYPE_LOG_ERROR,"\n%s", luaL_checkstring(lua, -1));
                }
                else
                    return;
            }
            lua_getglobal(this->lua, "loop");
            if (lua_isfunction(this->lua, -1))
            {
                lua_pushnumber(this->lua, this->device->delta);
                if (lua_pcall(this->lua, 1, 0, 0) != LUA_OK)
                {
                    this->wasError = true;
                    lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "loop", luaL_checkstring(lua, -1), this->scriptLua.c_str());
                }
				lua_settop(lua,0);
            }
            else
            {
                lua_pop(this->lua, 1);
            }

            // Call back timer
            for (unsigned int i = 0; i < this->dataScene.lsTimerCallBack.size(); ++i)
            {
                TIMER_CALL_BACK *ptrTimer = this->dataScene.lsTimerCallBack[i];
                if (ptrTimer->killTimer)
                {
                    this->dataScene.lsTimerCallBack.erase(this->dataScene.lsTimerCallBack.begin() + i);
                    if (ptrTimer->ref_MeAsTableTimer != LUA_NOREF)
                    {
                        luaL_unref(lua, LUA_REGISTRYINDEX, ptrTimer->ref_MeAsTableTimer);
                        ptrTimer->ref_MeAsTableTimer = LUA_NOREF;
                    }
                }
                else if (ptrTimer->isPaused)
                {
                    this->device->setTimer(ptrTimer->idTimer, ptrTimer->lastTimerElapsed);
                }
                else
                {
                    ptrTimer->lastTimerElapsed = this->device->getTimer(ptrTimer->idTimer);
                    if (ptrTimer->lastTimerElapsed >= ptrTimer->timerElapsed)
                    {
                        ptrTimer->times += 1;
                        this->device->setTimer(ptrTimer->idTimer, 0.0f);
                        lua_rawgeti(lua, LUA_REGISTRYINDEX, ptrTimer->ref_CallBackTimer);
                        if (lua_isfunction(this->lua, -1))
                        {
                            lua_rawgeti(lua, LUA_REGISTRYINDEX, ptrTimer->ref_MeAsTableTimer);
                            if (lua_pcall(this->lua, 1, 0, 0))
                            {
                                this->wasError = true;
                                lua_print_line(lua,TYPE_LOG_ERROR,"[%s]->\n[%s]", luaL_checkstring(lua, -1), this->scriptLua.c_str());
                            }
							lua_settop(lua,0);
                        }
                        else
                        {
                            lua_pop(this->lua, 1);
                        }
                    }
                }
            }

            while (this->dataScene.lsLuaCallBackStream.size())
            {
                USER_DATA_AUDIO_LUA *dataRenderer = this->dataScene.lsLuaCallBackStream[0];
                onEndStreamCallBackFromSceneThread(this->lua, dataRenderer);
                this->dataScene.lsLuaCallBackStream.erase(this->dataScene.lsLuaCallBackStream.begin());
            }

            while (this->dataScene.lsLuaCallBackOnTouchSynchronous.size())
            {
                auto *dataRenderer =
                    static_cast<USER_DATA_RENDER_LUA *>(this->dataScene.lsLuaCallBackOnTouchSynchronous[0]->userData);
                lua_rawgeti(lua, LUA_REGISTRYINDEX, dataRenderer->ref_CallBackTouchDown);
                if (lua_isfunction(this->lua, -1))
                {
                    lua_rawgeti(lua, LUA_REGISTRYINDEX, dataRenderer->ref_MeAsTable);
                    lua_pushinteger(this->lua, (int)dataRenderer->x);
                    lua_pushinteger(this->lua, (int)dataRenderer->y);
                    lua_pushinteger(this->lua, (int)dataRenderer->key);
                    if (lua_pcall(this->lua, 4, 0, 0))
                    {
                        this->wasError    = true;
                        const char *sErr1 = luaL_checkstring(lua, -1);
                        const char *sErr2 = this->scriptLua.c_str();
                        lua_print_line(lua,TYPE_LOG_ERROR,"[%s]->\n[%s]", sErr1, sErr2);
                    }
					lua_settop(lua,0);
                }
                else
                {
                    lua_pop(this->lua, 1);
                }
                this->dataScene.lsLuaCallBackOnTouchSynchronous.erase(
                    this->dataScene.lsLuaCallBackOnTouchSynchronous.begin());
            }

            if(this->time_resize_window > 0.0)//Window was resized. Lets wait a bit to resize it.
            {
                const float fps           = this->device->real_fps > 20.0f ? this->device->real_fps : 60.0f;
                const float real_delta    = (1.0f / fps);
                this->time_resize_window -= real_delta;
                if(this->time_resize_window <= 0.0f)
                {
                    auto newScene     = new SCENE_SCRIPT(this->getSceneName(), true, this->splashRenderizable);
                    auto *luaManager  = static_cast<LUA_MANAGER *>(device->ptrManager);
                    luaManager->lsScene.push_back(newScene);
                    this->device->scene->nextScene     = newScene;
                    this->device->scene->goToNextScene = true;
                    this->device->scene->endScene      = true;
                    this->time_resize_window           = 0.0f;
                }
            }
        }
        
        void SCENE_SCRIPT::onCallBackCommands(const char *functionNameCallBack,
                                const char *param)  // Callback de comandos nativos e ou interface - Java/ outros
        {
            if (functionNameCallBack && this->lua)
            {
                if (param)
                {
                    if (strcasecmp(param, "NULL") == 0)
                    {
                        if (luaL_dostring(lua, functionNameCallBack))
                        {
                            lua_print_line(lua,TYPE_LOG_ERROR,"\n%s", luaL_checkstring(lua, -1));
                            this->wasError = true;
                        }
                    }
                    else
                    {
                        lua_getglobal(this->lua, functionNameCallBack);
                        if (lua_isfunction(this->lua, -1))
                        {
                            lua_pushstring(this->lua, param);
                            if (lua_pcall(this->lua, 1, 0, 0))
                            {
                                this->wasError = true;
                                lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", functionNameCallBack, luaL_checkstring(lua, -1),this->scriptLua.c_str());
                            }
							lua_settop(lua,0);
                        }
                        else
                        {
                            const bool isAnyFunction = strchr(functionNameCallBack, ':') != nullptr || strchr(functionNameCallBack, '.') != nullptr;
                            if (isAnyFunction)
                            {
                                std::string doCommands(functionNameCallBack);
								doCommands += '(';
								doCommands += param;
								doCommands += ')';
                                if (luaL_dostring(lua, doCommands.c_str()))
                                {
                                    lua_print_line(lua,TYPE_LOG_ERROR,"\n%s", luaL_checkstring(lua, -1));
                                    this->wasError = true;
                                }
                            }
                            else
                            {
                                this->wasError = true;
                                lua_print_line(lua,TYPE_LOG_ERROR,"onCallBackCommands function [%s] not found!", functionNameCallBack);
                                lua_pop(this->lua, 1);
                            }
							lua_settop(lua,0);
                        }
                    }
                }
                else
                {
                    if (luaL_dostring(lua, functionNameCallBack))
                    {
                        lua_print_line(lua,TYPE_LOG_ERROR,"\n%s", luaL_checkstring(lua, -1));
                        this->wasError = true;
                    }
                }
            }
            else if (this->lua == nullptr)
            {
                lua_print_line(lua,TYPE_LOG_ERROR,"CallBack before init scene? -> [%s]",functionNameCallBack ? functionNameCallBack : "NULL");
            }
            else
            {
                lua_print_line(lua,TYPE_LOG_ERROR,"CallBack [NULL]??");
            }
        }
        
        void SCENE_SCRIPT::onRestore(const int percent) 
        {
            if (percent == 0)
            {
    #ifdef USE_OPENGL_ES
                GLClearDepthf(1.0f);
                GLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    #endif
                if (this->textureRestore == nullptr)
                {
                    this->textureRestore = new TEXTURE_VIEW(false,true);
                    if (this->textureRestore && this->textureRestore->load(& resource_loading))
                    {
                        this->textureRestore->position.x = this->device->getScaleBackBufferWidth() / 2.0f;
                        this->textureRestore->position.y = this->device->getScaleBackBufferHeight() / 2.0f;
                        this->textureRestore->scale.x    = 0.5f;
                        this->textureRestore->scale.y    = 0.5f;
                        this->device->renderToRestore(this->textureRestore);
                    }
                }
            }
            else
            {
                if (this->textureRestore)
                {
                    #ifdef USE_OPENGL_ES
                        GLClearColor(device->colorClearBackGround.r, device->colorClearBackGround.g, device->colorClearBackGround.b,
                                     device->colorClearBackGround.a);
                        GLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                        GLClearDepthf(1.0f);
                    #endif
                    this->textureRestore->angle.z = util::degreeToRadian((180.0f / 100.0f) * percent);
                    this->device->renderToRestore(this->textureRestore);
                }
                if(percent == 100)
                {
                    lua_getglobal(this->lua, "onRestore");
                    if (lua_isfunction(this->lua, -1))
                    {
                        if (lua_pcall(this->lua, 0, 0, 0))
                        {
                            this->wasError = true;
                            lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onRestore", luaL_checkstring(lua, -1),this->scriptLua.c_str());
                        }
						lua_settop(lua,0);
                    }
                    const int count = lua_gc(this->lua, LUA_GCCOUNT, 0);
                    const int ret   = lua_gc(this->lua, LUA_GCCOLLECT, 0);
                    const int clear = count - lua_gc(this->lua, LUA_GCCOUNT, 0);
                    if (ret)
                        lua_print_line(lua,TYPE_LOG_ERROR,"Errro at lua_gc code [%d]", ret);
                    else
                        INFO_LOG("LUA_GCCOLLECT:%d", clear);
                    if (this->textureRestore)
                    {
                        this->textureRestore->enableRender = false;
                        delete this->textureRestore;
                        this->textureRestore = nullptr;
                    }
                }
            }
        }
        
        void SCENE_SCRIPT::onTouchDown(int key, float x, float y)
        {
    #if !defined ANDROID
            key = key + 1;
    #endif
            lua_getglobal(this->lua, "onTouchDown");
            if (lua_isfunction(this->lua, -1))
            {
                lua_pushinteger(this->lua, key);
                lua_pushinteger(this->lua, (int)x);
                lua_pushinteger(this->lua, (int)y);
                if (lua_pcall(this->lua, 3, 0, 0))
                {
                    this->wasError = true;
                    lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onTouchDown", luaL_checkstring(lua, -1),this->scriptLua.c_str());
                }
				lua_settop(lua,0);
            }
            else
            {
                lua_pop(this->lua, 1);
            }
            for (auto obj : this->dataScene.lsLuaCallBackOnTouchAsynchronous)
            {
                if (!obj->enableRender)
                    continue;
                auto *dataRenderer = static_cast<USER_DATA_RENDER_LUA *>(obj->userData);
                if (obj->is3D)
                {
                    if (obj->isOver3d(this->device, x, y))
                    {
                        dataRenderer->x   = x;
                        dataRenderer->y   = y;
                        dataRenderer->key = key;
                        this->dataScene.lsLuaCallBackOnTouchSynchronous.push_back(obj);
                    }
                }
                else if (obj->is2dS)
                {
                    if (obj->isOver2ds(this->device, x, y))
                    {
                        dataRenderer->x   = x;
                        dataRenderer->y   = y;
                        dataRenderer->key = key;
                        this->dataScene.lsLuaCallBackOnTouchSynchronous.push_back(obj);
                    }
                }
                else
                {
                    if (obj->isOver2dw(this->device, x, y))
                    {
                        dataRenderer->x   = x;
                        dataRenderer->y   = y;
                        dataRenderer->key = key;
                        this->dataScene.lsLuaCallBackOnTouchSynchronous.push_back(obj);
                    }
                }
            }
        }
        
        void SCENE_SCRIPT::onTouchUp(int key, float x, float y) 
        {
            lua_getglobal(this->lua, "onTouchUp");
            if (lua_isfunction(this->lua, -1))
            {
    #ifdef ANDROID
                lua_pushinteger(this->lua, key);
    #else
                lua_pushinteger(this->lua, key + 1);
    #endif
                lua_pushinteger(this->lua, (int)x);
                lua_pushinteger(this->lua, (int)y);
                if (lua_pcall(this->lua, 3, 0, 0))
                {
                    this->wasError = true;
                    lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onTouchUp", luaL_checkstring(lua, -1),this->scriptLua.c_str());
                }
				lua_settop(lua,0);
            }
            else
            {
                lua_pop(this->lua, 1);
            }
        }
        //#define STACK_TRACE_DEBUG 1
        
        void SCENE_SCRIPT::onTouchMove(int key, float x, float y) 
        {
    #if defined       STACK_TRACE_DEBUG // sample how to do a trace debug calling from C
            const int errfunc = 1;
            lua_getglobal(lua, "debug");
            lua_getfield(lua, -1, "traceback");
            lua_replace(lua, -2);
    #else
            const int errfunc = 0;
    #endif

            lua_getglobal(this->lua, "onTouchMove");
            if (lua_isfunction(this->lua, -1))
            {

    #ifdef ANDROID
                lua_pushinteger(this->lua, key);
    #else
                lua_pushinteger(this->lua, key + 1);
    #endif
                lua_pushinteger(this->lua, (int)x);
                lua_pushinteger(this->lua, (int)y);
                if (lua_pcall(this->lua, 3, 0, errfunc))
                {
                    this->wasError = true;
                    lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onTouchMove", luaL_checkstring(lua, -1),this->scriptLua.c_str());
                    lua_settop(lua, 0);
                }
    #if defined STACK_TRACE_DEBUG
                else
                {
                    lua_pop(this->lua, 1);
                }
    #endif
            }
            else
            {
                lua_pop(this->lua, 1);
            }
        }
        
        void SCENE_SCRIPT::onTouchZoom(float zoom) 
        {
            lua_getglobal(this->lua, "onTouchZoom");
            if (lua_isfunction(this->lua, -1))
            {
                lua_pushinteger(this->lua, (int)zoom);
                if (lua_pcall(this->lua, 1, 0, 0))
                {
                    this->wasError = true;
                    lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onTouchZoom", luaL_checkstring(lua, -1),this->scriptLua.c_str());
                }
				lua_settop(lua,0);
            }
            else
            {
                lua_pop(this->lua, 1);
            }
        }

    #if !defined     ANDROID
        void SCENE_SCRIPT::onDoubleClick(float x, float y,int key)  // Double click do mouse. key ==0 bot�o esquerdo; key == 1 bot�o direito.
        {
            if (this->lua)
            {
                lua_getglobal(this->lua, "onDoubleClick");
                if (lua_isfunction(this->lua, -1))
                {
                    lua_pushinteger(this->lua, (int)key);
                    lua_pushinteger(this->lua, (int)x);
                    lua_pushinteger(this->lua, (int)y);
                    if (lua_pcall(this->lua, 3, 0, 0))
                    {
                        this->wasError = true;
                        lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onDoubleClick", luaL_checkstring(lua, -1),this->scriptLua.c_str());
                    }
					lua_settop(lua,0);
                }
                else
                    lua_pop(this->lua, 1);
            }
        }
    #endif
        
        void SCENE_SCRIPT::onFinalizeScene() 
        {
            if (this->lua)
            {
                lua_getglobal(this->lua, "onEndScene");
                if (lua_isfunction(this->lua, -1))
                {
                    if (lua_pcall(this->lua, 0, 0, 0))
                    {
                        this->wasError = true;
                        lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onEndScene", luaL_checkstring(lua, -1),this->scriptLua.c_str());
                    }
					lua_settop(lua,0);
                }
                else
                    lua_pop(this->lua, 1);
            }
        }
        
        void SCENE_SCRIPT::onKeyDown(int key) 
        {
            lua_getglobal(this->lua, "onKeyDown");
            if (lua_isfunction(this->lua, -1))
            {
                lua_pushinteger(this->lua, (int)key);
                if (lua_pcall(this->lua, 1, 0, 0))
                {
                    this->wasError = true;
                    lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onKeyDown", luaL_checkstring(lua, -1),this->scriptLua.c_str());
                }
				lua_settop(lua,0);
            }
            else
            {
                lua_pop(this->lua, 1);
            }
        }
        
        void SCENE_SCRIPT::onKeyUp(int key) 
        {
            lua_getglobal(this->lua, "onKeyUp");
            if (lua_isfunction(this->lua, -1))
            {
                lua_pushinteger(this->lua, (int)key);
                if (lua_pcall(this->lua, 1, 0, 0))
                {
                    this->wasError = true;
                    lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onKeyUp", luaL_checkstring(lua, -1),this->scriptLua.c_str());
                }
				lua_settop(lua,0);
            }
            else
            {
                lua_pop(this->lua, 1);
            }
        }
        
        void SCENE_SCRIPT::onKeyDownJoystick(int player, int key) 
        {
            lua_getglobal(this->lua, "onKeyDownJoystick");
            if (lua_isfunction(this->lua, -1))
            {
                lua_pushinteger(this->lua, player);
                lua_pushinteger(this->lua, key);
                if (lua_pcall(this->lua, 2, 0, 0))
                {
                    lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onKeyDownJoystick", luaL_checkstring(lua, -1),this->scriptLua.c_str());
                }
				lua_settop(lua,0);
            }
            else
            {
                lua_pop(this->lua, 1);
            }
        }
        
        void SCENE_SCRIPT::onKeyUpJoystick(int player, int key)  // parameter: int player, int key
        {
            lua_getglobal(this->lua, "onKeyUpJoystick");
            if (lua_isfunction(this->lua, -1))
            {
                lua_pushinteger(this->lua, player);
                lua_pushinteger(this->lua, key);
                if (lua_pcall(this->lua, 2, 0, 0))
                {
                    lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onKeyUpJoystick", luaL_checkstring(lua, -1),this->scriptLua.c_str());
                }
				lua_settop(lua,0);
            }
            else
            {
                lua_pop(this->lua, 1);
            }
        }
        
        void SCENE_SCRIPT::onMoveJoystick(int player, float lx, float ly, float rx,float ry)  // parameter: int player, float lx, float ly, float rx, float ry
        {
            lua_getglobal(this->lua, "onMoveJoystick");
            if (lua_isfunction(this->lua, -1))
            {
                lua_pushinteger(this->lua, player);
                lua_pushnumber(this->lua, lx);
                lua_pushnumber(this->lua, ly);
                lua_pushnumber(this->lua, rx);
                lua_pushnumber(this->lua, ry);
                if (lua_pcall(this->lua, 5, 0, 0))
                {
                    lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onMoveJoystick", luaL_checkstring(lua, -1),this->scriptLua.c_str());
                }
				lua_settop(lua,0);
            }
            else
            {
                lua_pop(this->lua, 1);
            }
        }
        
        void SCENE_SCRIPT::onInfoDeviceJoystick(int player, int maxNumberButton, const char *strDeviceName, const char *extraInfo)
             // parameter: int player, int maxNumberButton, const char* strDeviceName, const char* extraInfo
        {
            lua_getglobal(this->lua, "onInfoJoystick");
            if (lua_isfunction(this->lua, -1))
            {
                lua_pushinteger(this->lua, player);
                lua_pushinteger(this->lua, maxNumberButton);
                lua_pushstring(this->lua, strDeviceName);
                lua_pushstring(this->lua, extraInfo);
                if (lua_pcall(this->lua, 4, 0, 0))
                {
                    lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onInfoJoystick", luaL_checkstring(lua, -1),this->scriptLua.c_str());
                }
				lua_settop(lua,0);
            }
            else
            {
                lua_pop(this->lua, 1);
            }
        }
        
        void SCENE_SCRIPT::startLoading() 
        {
			if(this->splashRenderizable)
			{
				if(this->textureLogo)
					delete this->textureLogo;
				this->textureLogo = nullptr;
				this->splashRenderizable->enableRender = true;
				RENDERIZABLE* oldRenderizable = this->splashRenderizable;
				this->splashRenderizable = mbm::clone(this,oldRenderizable);//the new 
				delete oldRenderizable;
				this->device->disableAllButThis(this->splashRenderizable);
			}
			else if (this->textureLogo == nullptr)
            {
                this->textureLogo                  = new TEXTURE_VIEW(this, true, true);
                this->device->colorClearBackGround = COLOR(1.0f,1.0f,1.0f,1.0f);
                if (this->textureLogo && this->textureLogo->load(& resource_loading))
                {
                    this->textureLogo->position.x = this->device->getScaleBackBufferWidth() / 2.0f;
                    this->textureLogo->position.y = this->device->getScaleBackBufferHeight() / 2.0f;
					this->textureLogo->position.z = -1000;
                    this->textureLogo->scale.x    = 0.5f;
                    this->textureLogo->scale.y    = 0.5f;
                    this->device->disableAllButThis(this->textureLogo);
                }
            }
		}

		void SCENE_SCRIPT::endLoading()
		{
			if(this->splashRenderizable)
			{
				/*
				--Same as the following code:
				
				if cCoroutineLoadScene and type(cCoroutineLoadScene) ~= 'coroutine' then 
					local tSplash = mbm.getSplash()		
					if tSplash then
						tSplash.visible = false
					end
				end
				*/
				lua_getglobal(lua,"cCoroutineLoadScene");
				const int type  = lua_type(lua,-1);
				if (type != LUA_TTHREAD)
					this->splashRenderizable->enableRender = false;
				
				lua_settop(lua,0);
			}
		}

        void SCENE_SCRIPT::onResizeWindow()
        {
            lua_getglobal(this->lua, "onResizeWindow");
			if (lua_isfunction(this->lua, -1))
			{
				if (lua_pcall(this->lua, 0, 0, 0))
				{
					this->wasError = true;
					lua_print_line(lua,TYPE_LOG_ERROR,"[%s] ->\n[%s]->\n[%s]", "onResizeWindow", luaL_checkstring(lua, -1),this->scriptLua.c_str());
				}
				lua_settop(lua,0);
			}
			else
			{
				this->time_resize_window = 1.0f;//restart the scene after 1 second
			}
            lua_settop(lua,0);
        }
        
        const char * SCENE_SCRIPT::getSceneName() noexcept
        {
            return this->fileNameScriptLuaFinal.c_str();
        }

        bool SCENE_SCRIPT::doFileAsString(const char *newScriptPath)
        {
            FILE *fp = nullptr;
            if (this->scriptLua.size() && newScriptPath)
                fp = util::openFile(newScriptPath, "rt");
            else
                fp = util::openFile("main.lua", "rt");
            if (fp == nullptr)
                fp = util::openFile("main.lua", "rt");
            if (fp)
            {
    #if _DEBUG
                INFO_LOG("file %s sucess opened!", this->scriptLua.c_str());
    #endif
                char        line[4096];
                std::string script_str;
                while (fgets(line, sizeof(line), fp))
                {
                    script_str += line;
                    script_str += "\n";
                }
                if (!luaL_dostring(this->lua, script_str.c_str()))
                {
                    fclose(fp);
                    return true;
                }
                else
                {
                    lua_print_line(lua,TYPE_LOG_ERROR,"\n%s", luaL_checkstring(lua, -1));
                    this->wasError = true;
                    fclose(fp);
                    return false;
                }
            }
            else if (newScriptPath)
            {
                this->wasError = true;
                lua_print_line(lua,TYPE_LOG_ERROR,"error on open file %s!", newScriptPath);
                return false;
            }
            else
            {
                this->wasError = true;
                lua_print_line(lua,TYPE_LOG_ERROR,"error on open file %s!", "main.lua");
                return false;
            }
        }
        
        bool SCENE_SCRIPT::createSceneLua()
        {
            if (this->lua)
                lua_close(this->lua);
            this->lua = nullptr;
            this->lua = luaL_newstate();
            if (this->lua == nullptr)
                return false;
            luaL_openlibs(this->lua);
            registerNamespaceMBM(this->lua, this, onNewScene, OnGetSplash);
            std::vector<std::string> allPath;
            util::getAllPaths(allPath);
            for(const std::string & path : allPath)
            {
                LUA_MANAGER::onAddPathScript(path.c_str());
            }
            return true;
        }
        
        void SCENE_SCRIPT::removeNullFromList(std::vector<RENDERIZABLE*> &myList)
        {
            for (unsigned int i = 0; i < myList.size(); i++)
            {
                RENDERIZABLE *_obj = myList[i];
                if (_obj == nullptr)
                {
                    myList.erase(myList.begin() + i);
                    this->removeNullFromList(myList);
                    break;
                }
            }
        }

		bool SCENE_SCRIPT::doLauncher(const std::string& fileNameScene)
		{
			mbm::DYNAMIC_VAR* var = device->lsDynamicVarGlobal["fileNameScene"];
			if (var == nullptr)
			{
				var = new DYNAMIC_VAR(DYNAMIC_CSTRING, fileNameScene.c_str());
				device->lsDynamicVarGlobal["fileNameScene"] = var;
			}
			else if (var->type != DYNAMIC_CSTRING)
			{
				delete var;
				var = new DYNAMIC_VAR(DYNAMIC_CSTRING, fileNameScene.c_str());
				device->lsDynamicVarGlobal["fileNameScene"] = var;
			}
			else
			{
				var->setString(fileNameScene.c_str());
			}
			if (luaL_dostring(this->lua, p_launcherLua))
			{
				lua_print_line(lua,TYPE_LOG_ERROR,"\n%s", luaL_checkstring(lua, -1));
				return false;
			}
			return true;
		}
        

    #ifdef ANDROID
        
        LUA_MANAGER::LUA_MANAGER(JNIEnv *env, jobject obj)
        {
            LUA_MANAGER::pLuaManager = this;
			log_util::setScriptPrintLine(onScriptPrintLine);
			util::setOnAddPathScript(onAddPathScript);
    #ifdef USE_OPENGL_ES
            this->nameAplication = "Mini-mbm " MBM_VERSION " GLES";
    #else
            this->nameAplication = "Mini-mbm " MBM_VERSION;
    #endif
            this->nameAplication += "\n Compiled: " __DATE__;
            this->widthWindow        = 800;
            this->heightWindow       = 600;
            this->positionXWindow    = 1;
            this->positionYWindow    = 1;
            this->maximizedWindow    = false;
            this->fileNameInitialLua = "main.lua";
    #if defined _DEBUG
            this->noSplash = true;
    #else
            this->noSplash       = false;
    #endif
			this->noBorder		=	false;
            this->device->jni->jenv = env;
			this->hasValueTextureLogo = false;
			INFO_LOG("%s", this->nameAplication.c_str());
        }
    #elif defined _WIN32 || defined __linux__
        
        LUA_MANAGER::LUA_MANAGER()
        {
            LUA_MANAGER::pLuaManager = this;
            onDoNativeCommand = nullptr;
			log_util::setScriptPrintLine(onScriptPrintLine);
			util::setOnAddPathScript(onAddPathScript);
    #if defined USE_OPENGL_ES
            this->nameAplication = "Mini-mbm " MBM_VERSION" GLES";
    #else
            this->nameAplication = "Mini-mbm " MBM_VERSION;
    #endif
            this->nameAplication += " Compiled: " __DATE__;
    #if defined _WIN32
            int _w = 0;
            int _h = 0;
            util::getDisplayMetrics(&_w,&_h);
            this->widthWindow  = _w;
            this->heightWindow = _h;
            this->positionXWindow = 0xffffff;//0xffffff means centralize
            this->positionYWindow = 0xffffff;//0xffffff means centralize 
    #else
            this->widthWindow    = 800;
            this->heightWindow   = 600;
            this->positionXWindow = 0;
            this->positionYWindow = 0;
    #endif
            
            this->maximizedWindow = false;
    #if defined _DEBUG
            this->noSplash        = true;
    #else
            this->noSplash       = false;
    #endif
			this->noBorder		= false;
			this->hasValueTextureLogo = false;
			INFO_LOG("%s", this->nameAplication.c_str());
        }

        LUA_MANAGER::LUA_MANAGER(const std::vector<std::string> & args)
        {
            LUA_MANAGER::pLuaManager = this;
            onDoNativeCommand = nullptr;
			log_util::setScriptPrintLine(onScriptPrintLine);
			util::setOnAddPathScript(onAddPathScript);
            this->nameAplication = "Mini-mbm " MBM_VERSION;
            this->nameAplication += " Compiled: " __DATE__;
    #if defined _WIN32
            int _w = 0;
            int _h = 0;
            util::getDisplayMetrics(&_w,&_h);
            this->widthWindow = _w;
            this->heightWindow = _h;
            this->positionXWindow = 0xffffff;//0xffffff means centralize
            this->positionYWindow = 0xffffff;//0xffffff means centralize 
    #else
            this->widthWindow    = 800;
            this->heightWindow   = 600;
            this->positionXWindow = 0;
            this->positionYWindow = 0;
    #endif
            
            this->maximizedWindow = false;
    #if defined _DEBUG
            this->noSplash        = true;
    #else
            this->noSplash       = false;
    #endif
			this->noBorder = false;
			DEVICE* tDevice = DEVICE::getInstance();
            if(args.size() > 0)
            {
                auto  dExeName = new DYNAMIC_VAR(DYNAMIC_CSTRING,args[0].c_str());
                tDevice->lsDynamicVarGlobal["_executable_name_"] = dExeName;
            }
            
			this->hasValueTextureLogo = false;
            this->parserArgs(args);
			if(this->device->verbose)
				INFO_LOG("%s", this->nameAplication.c_str());
        }

        LUA_MANAGER::LUA_MANAGER(const int argc, const char **argv)
        {
            LUA_MANAGER::pLuaManager = this;
            onDoNativeCommand = nullptr;
			log_util::setScriptPrintLine(onScriptPrintLine);
			util::setOnAddPathScript(onAddPathScript);
            this->nameAplication = "Mini-mbm " MBM_VERSION;
            this->nameAplication += " Compiled: " __DATE__;
    #if defined _WIN32
            int _w = 0;
            int _h = 0;
            util::getDisplayMetrics(&_w,&_h);
            this->widthWindow = _w;
            this->heightWindow = _h;
            this->positionXWindow = 0xffffff;//0xffffff means centralize
            this->positionYWindow = 0xffffff;//0xffffff means centralize 
    #else
            this->widthWindow    = 800;
            this->heightWindow   = 600;
            this->positionXWindow = 0;
            this->positionYWindow = 0;
    #endif
            
            this->maximizedWindow = false;
    #if defined _DEBUG
            this->noSplash        = true;
    #else
            this->noSplash       = false;
    #endif
			this->noBorder = false;
			std::vector<std::string> lsArg;
            DEVICE* tDevice = DEVICE::getInstance();

            auto  dExeName = new DYNAMIC_VAR(DYNAMIC_CSTRING,argv[0]);
            tDevice->lsDynamicVarGlobal["_executable_name_"] = dExeName;
            
			this->hasValueTextureLogo = false;
            lsArg.reserve(argc);
            for (int i = 0; i < argc; ++i)
            {
                lsArg.emplace_back(argv[i]);
            }
            this->parserArgs(lsArg);
			if(this->device->verbose)
				INFO_LOG("%s", this->nameAplication.c_str());
        }
    #endif
        
        LUA_MANAGER::~LUA_MANAGER()
        {
            for (auto newScene : this->lsScene)
            {
                delete newScene;
            }
            this->lsScene.clear();
            LUA_MANAGER::pLuaManager = nullptr;
        }

		bool LUA_MANAGER::existScene(const int idScene)
		{
			for (auto ptrScene : lsScene)
			{
					if(ptrScene && ptrScene->getIdScene() == idScene)
					return true;
			}
			return false;
		}
        void LUA_MANAGER::setExpectedSizeOfWindow(int expectedWidth,int expectedHeight,const char * stretch)
        {
            DYNAMIC_VAR * D_expectedW = this->device->lsDynamicVarGlobal["expectedwidth"];
            DYNAMIC_VAR * D_expectedH = this->device->lsDynamicVarGlobal["expectedheight"];
            if(D_expectedW == nullptr)
                this->device->lsDynamicVarGlobal["expectedwidth"] = new DYNAMIC_VAR(DYNAMIC_INT,&expectedWidth);
            else if(D_expectedW->type == DYNAMIC_INT)
                D_expectedW->setInt(expectedWidth);
            else if(D_expectedW->type == DYNAMIC_FLOAT)
                D_expectedW->setFloat(static_cast<float>(expectedWidth));

            if(D_expectedH == nullptr)
                this->device->lsDynamicVarGlobal["expectedheight"] = new DYNAMIC_VAR(DYNAMIC_INT,&expectedHeight);
            else if(D_expectedH->type == DYNAMIC_INT)
                D_expectedH->setInt(expectedHeight);
            else if(D_expectedH->type == DYNAMIC_FLOAT)
                D_expectedH->setFloat(static_cast<float>(expectedHeight));
            if(stretch)
            {
                DYNAMIC_VAR * D_scaleTo = this->device->lsDynamicVarGlobal["stretch"];
                if(D_scaleTo == nullptr)
                    this->device->lsDynamicVarGlobal["stretch"] = new DYNAMIC_VAR(DYNAMIC_CSTRING,stretch);
                else if(D_scaleTo->type == DYNAMIC_CSTRING)
                    D_scaleTo->setString(stretch);
            }
        }

		void LUA_MANAGER::getExpectedSizeOfWindow(int & expectedWidth,int & expectedHeight, std::string & stretch)
		{
			DYNAMIC_VAR * D_expectedW = this->device->lsDynamicVarGlobal["expectedwidth"];
            DYNAMIC_VAR * D_expectedH = this->device->lsDynamicVarGlobal["expectedheight"];
            if(D_expectedW)
			{
				if(D_expectedW->type == DYNAMIC_INT)
					expectedWidth = D_expectedW->getInt();
				else if(D_expectedW->type == DYNAMIC_FLOAT)
					expectedWidth = static_cast<int>(D_expectedW->getFloat());
			}

            if(D_expectedH)
			{
				if(D_expectedH->type == DYNAMIC_INT)
					expectedHeight = D_expectedH->getInt();
				else if(D_expectedH->type == DYNAMIC_FLOAT)
					expectedHeight = static_cast<int>(D_expectedH->getFloat());
			}
            DYNAMIC_VAR * D_scaleTo = this->device->lsDynamicVarGlobal["stretch"];
            if(D_scaleTo && D_scaleTo->type == DYNAMIC_CSTRING)
				stretch = D_scaleTo->getString();
		}
    #ifdef ANDROID
        
        bool LUA_MANAGER::initializeSceneLua(int w, int h,int _expectedWidth,int _expectedHeight)
        {
            this->widthWindow  = w;
            this->heightWindow = h;
            std::string s_stretch("y");
    #else
        bool LUA_MANAGER::initializeSceneLua(const bool border)
        {
            this->device->ptrManager = this;
	        int _expectedWidth = 1024;
	        int _expectedHeight= 768;
            std::string s_stretch("y");
	    getExpectedSizeOfWindow(_expectedWidth,_expectedHeight,s_stretch);
    #endif
	    setExpectedSizeOfWindow(_expectedWidth,_expectedHeight,s_stretch.c_str());
    #if defined ANDROID
            if (this->initGl(this->widthWindow, this->heightWindow))
    #elif defined _WIN32
            if (this->initGl(this->nameAplication.c_str(), this->widthWindow, this->heightWindow, this->positionXWindow,this->positionYWindow, border,this->enableResizeWindow))
    #elif defined __linux__
            if (this->initGl(this->nameAplication.c_str(), this->widthWindow, this->heightWindow, border))
    #else
        #error "undefined platform"
    #endif
            {
                if (this->fileNameInitialLua.size())
                {
                    this->fileNameInitialLua = util::getFullPath(this->fileNameInitialLua.c_str(),nullptr);
                    auto newScene   = new SCENE_SCRIPT(this->fileNameInitialLua.c_str(), this->noSplash,nullptr);
					#if defined _WIN32 //issue Visual studio!!!
						static bool sleepFirsTime = true;
						if (sleepFirsTime)
						{
							sleepFirsTime = false;
							Sleep(200);
						}
					#endif
                    this->lsScene.push_back(newScene);
                    this->setScene(newScene);
                }
                return true;
            }
            return false;
        }
        
        int LUA_MANAGER::run()
        {
            if (this->device->scene && this->lsScene.size()) // is there an initial scene?
            {
                SCENE_SCRIPT *newScene = this->lsScene[0];
                this->setScene(newScene);
            }
            else
            {
                auto newScene = new SCENE_SCRIPT("main.lua", this->noSplash, nullptr);
                this->lsScene.push_back(newScene);
                this->setScene(newScene);
            }
    #if defined _WIN32 || (defined __linux__ && !defined ANDROID)
    #ifdef USE_OPENGL_ES
            this->loop();
    #else
            this->enterLoop();
    #endif
    #endif
            return 0;
        }
        
        void LUA_MANAGER::parserArgs(const std::vector<std::string> &argv)
        {
            ARGS_LUA nextArg = NONE;
            ARGS_LUA lastArg = NONE;
            
            for (unsigned int i = 0; i < argv.size(); ++i)
            {
                const char* arg = argv[i].c_str();
                if (strcasecmp(arg, "-w") == 0 || strcasecmp(arg, "-width") == 0 || strcasecmp(arg, "--width") == 0)
                    nextArg = WIDTH_SCREEN;
                else if (strcasecmp(arg, "-ew") == 0 || strcasecmp(arg, "-expectedwidth") == 0 || strcasecmp(arg, "--expectedwidth") == 0)
                    nextArg = EXPECTED_WIDTH_SCREEN;
                else if (strcasecmp(arg, "--nosplash") == 0 || strcasecmp(arg, "-nosplash") == 0)
                {
                    nextArg        = NONE;
                    this->noSplash = true;
                }
                else if (strcasecmp(arg, "-h") == 0 || strcasecmp(arg, "-height") == 0 || strcasecmp(arg, "--height") == 0)
                    nextArg = HEIGHT_SCREEN;
                else if (strcasecmp(arg, "-eh") == 0 || strcasecmp(arg, "-expectedheight") == 0 || strcasecmp(arg, "--expectedheight") == 0)
                    nextArg = EXPECTED_HEIGHT_SCREEN;
                else if (strcasecmp(arg, "--posx") == 0 || strcasecmp(arg, "-x") == 0 || strcasecmp(arg, "-posx") == 0)
                    nextArg = POSITION_X_SCREEN;
                else if (strcasecmp(arg, "--posy") == 0 || strcasecmp(arg, "-y") == 0 || strcasecmp(arg, "-posy") == 0)
                    nextArg = POSITION_Y_SCREEN;
                else if (strcasecmp(arg, "--maximizedwindow") == 0 || strcasecmp(arg, "-maximizedwindow") == 0)
                    nextArg = MAXIMIZED_WINDOW;
                else if (strcasecmp(arg, "--scene") == 0 || strcasecmp(arg, "-s") == 0 || strcasecmp(arg, "-scene") == 0)
                    nextArg = INITIAL_SCENE_LUA;
                else if (strcasecmp(arg, "--name") == 0 || strcasecmp(arg, "--n") == 0 || strcasecmp(arg, "-name") == 0)
                    nextArg = NAME_APP;
                else if (strcasecmp(arg, "-a") == 0 || strcasecmp(arg, "--addpath") == 0 || strcasecmp(arg, "-addpath") == 0)
                    nextArg = ADD_PATH;
                else if (strcasecmp(arg, "-e") == 0 || strcasecmp(arg, "--execute") == 0 || strcasecmp(arg, "-execute") == 0)
                    nextArg = EXECUTE_STRING;
                else if (strcasecmp(arg, "-enableResizeWindow") == 0 || strcasecmp(arg, "--enableResizeWindow") == 0)
                    nextArg = ENABLE_RESIZE_WINDOW;
				else if (strcasecmp(arg, "-notverbose") == 0 || strcasecmp(arg, "--notverbose") == 0)
				{
					this->device->verbose = false;
					nextArg = NONE;
				}
                else if (strcasecmp(arg, "-verbose") == 0 || strcasecmp(arg, "--verbose") == 0)
				{
					this->device->verbose = true;
					nextArg = NONE;
				}
				else if (strcasecmp(arg, "--noborder") == 0 || strcasecmp(arg, "-noborder") == 0)
				{
					nextArg = NO_BORDER;
					noBorder = true;
				}
                else if(nextArg == EXECUTE_STRING)
                {
                     this->string_to_execute= argv[i];
                     nextArg        = NONE;
                }
                else if(i == 0)//first arg must be the executable name (just ignore)
                {
                    nextArg        = NONE;
                }
                else
                {
                    if (argv[i].find('=') != std::string::npos)
                        nextArg = NONE;
                    switch (nextArg)
                    {
                        case NONE:
                        {
                            std::vector<std::string> result;
                            util::split(result, arg, '=');
                            if (result.size() == 2)
                            {
                                const char *      name  = result[0].c_str();
                                const char *      value = result[1].c_str();
                                DYNAMIC_VAR *var   = this->device->lsDynamicVarGlobal[name];
                                if (var)
                                    delete var;
                                auto vFloat    = static_cast<float>(std::atof(value));
                                int vInt        = std::atoi(value);
                                if(vFloat != 0.0f) //-V550
                                    device->lsDynamicVarGlobal[name] = new DYNAMIC_VAR(DYNAMIC_FLOAT,&vFloat);
                                else if(vInt != 0)
                                    device->lsDynamicVarGlobal[name] = new DYNAMIC_VAR(DYNAMIC_INT,&vInt);
                                else
                                    device->lsDynamicVarGlobal[name] = new DYNAMIC_VAR(DYNAMIC_CSTRING,value);
                            }
                            else
                            {
                                std::string sarg(arg);
                                bool foundLua = false;
                                util::split(result, arg, '.');
                                if (result.size() >= 2 && result[result.size() - 1].compare("lua") == 0 && 
                                    this->fileNameInitialLua.size() == 0 &&
                                    sarg.find('=') == std::string::npos)
                                {
                                    this->fileNameInitialLua = argv[i];
                                    foundLua                 = true;
                                }
                                switch (lastArg)
                                {
                                    case NONE:
                                    {
                                        if (foundLua == false)
                                        {
                                            util::split(result, arg, '=');
                                            if (result.size() == 2)
                                            {
                                                const char *      name  = result[0].c_str();
                                                const char *      value = result[1].c_str();
                                                DYNAMIC_VAR *var   = this->device->lsDynamicVarGlobal[name];
                                                if (var)
                                                    delete var;
                                                auto vFloat    = static_cast<float>(std::atof(value));
                                                int vInt        = std::atoi(value);
                                                if(vFloat != 0.0f) //-V550
                                                    var = new DYNAMIC_VAR(DYNAMIC_FLOAT,&vFloat);
                                                else if(vInt != 0)
                                                    var = new DYNAMIC_VAR(DYNAMIC_INT,&vInt);
                                                else
                                                    var = new DYNAMIC_VAR(DYNAMIC_CSTRING,value);
                                                device->lsDynamicVarGlobal[name] = var;
                                            }
                                        }

                                        nextArg = NONE;
                                    }
                                    break;
                                    case NO_SPLASH:
                                    {
                                        this->noSplash = ((unsigned int)std::atoi(arg)) ? true : false;
                                        nextArg        = NO_SPLASH;
                                    }
                                    break;
									case NO_BORDER:
									{
										noBorder = true;
										nextArg = NONE;
									}
									break;
                                    case ENABLE_RESIZE_WINDOW:
                                    {
                                        enableResizeWindow = ((unsigned int)std::atoi(arg)) ? true : false;
                                        nextArg = NONE;
                                    }
                                    break;
                                    case EXECUTE_STRING:
                                    {
                                        nextArg  = NONE;
                                    }
                                    break;
                                    case WIDTH_SCREEN:
                                    {
                                        auto newW = (unsigned int)std::atoi(arg);
                                        if (newW > 0)
                                            this->widthWindow = newW;
                                        nextArg               = WIDTH_SCREEN;
                                    }
                                    break;
                                    case HEIGHT_SCREEN:
                                    {
                                        auto newH = (unsigned int)std::atoi(arg);
                                        if (newH > 0)
                                            this->heightWindow = newH;
                                        nextArg                = HEIGHT_SCREEN;
                                    }
                                    break;
                                    case EXPECTED_WIDTH_SCREEN:
                                    {
                                        int newW = std::atoi(arg);
                                        if (newW > 0)
                                        {
                                            DYNAMIC_VAR * D_expectedW = this->device->lsDynamicVarGlobal["expectedwidth"];
                                            if(D_expectedW == nullptr)
                                                this->device->lsDynamicVarGlobal["expectedwidth"] = new DYNAMIC_VAR(DYNAMIC_INT,&newW);
                                            else if(D_expectedW->type == DYNAMIC_INT)
                                                D_expectedW->setInt(newW);
                                            else if(D_expectedW->type == DYNAMIC_FLOAT)
                                                D_expectedW->setFloat(static_cast<float>(newW));
                                        }
                                        nextArg               = EXPECTED_WIDTH_SCREEN;
                                    }
                                    break;
                                    case EXPECTED_HEIGHT_SCREEN:
                                    {
                                        int newH = std::atoi(arg);
                                        if (newH > 0)
                                        {
                                            DYNAMIC_VAR * D_expectedH = this->device->lsDynamicVarGlobal["expectedheight"];
                                            if(D_expectedH == nullptr)
                                                this->device->lsDynamicVarGlobal["expectedheight"] = new DYNAMIC_VAR(DYNAMIC_INT,&newH);
                                            else if(D_expectedH->type == DYNAMIC_INT)
                                                D_expectedH->setInt(newH);
                                            else if(D_expectedH->type == DYNAMIC_FLOAT)
                                                D_expectedH->setFloat(static_cast<float>(newH));
                                        }
                                        nextArg                = EXPECTED_HEIGHT_SCREEN;
                                    }
                                    break;
                                    case POSITION_X_SCREEN:
                                    {
                                        this->positionXWindow = (unsigned int)std::atoi(arg);
                                        nextArg               = POSITION_X_SCREEN;
                                    }
                                    break;
                                    case POSITION_Y_SCREEN:
                                    {
                                        this->positionYWindow = (unsigned int)std::atoi(arg);
                                        nextArg               = POSITION_Y_SCREEN;
                                    }
                                    break;
                                    case MAXIMIZED_WINDOW:
                                    {
                                        this->maximizedWindow = std::atoi(arg) ? true : false;
                                        nextArg               = MAXIMIZED_WINDOW;
                                    }
                                    break;
                                    case INITIAL_SCENE_LUA:
                                    {
                                        this->fileNameInitialLua = argv[i];
                                        nextArg                  = INITIAL_SCENE_LUA;
                                    }
                                    break;
                                    case NAME_APP: 
                                    {   
                                        if(this->nameAplication.size())
                                            this->nameAplication += " ";
                                        this->nameAplication += argv[i];
                                    }
                                    break;
                                    case ADD_PATH: { util::addPath(arg);}
                                    break;
                                    default:
                                    {
                                        nextArg = NONE;
                                        WARN_LOG("\nArgument: %s %s", arg, " ignored\r\n");
                                    }
                                    break;
                                }
                            }
                        }
                        break;
                        case NO_SPLASH: { this->noSplash = ((unsigned int)std::atoi(arg)) ? true : false;}
                        break;
						case NO_BORDER:
						{
							noBorder = true;
							nextArg = NONE;
						}
						break;
                        case ENABLE_RESIZE_WINDOW:
                        {
                            enableResizeWindow = ((unsigned int)std::atoi(arg)) ? true : false;
                            nextArg = NONE;
                        }
                        break;
                        case WIDTH_SCREEN:
                        {
                            auto newW = (unsigned int)std::atoi(arg);
                            if (newW > 0)
                                this->widthWindow = newW;
                        }
                        break;
                        case HEIGHT_SCREEN:
                        {
                            auto newH = (unsigned int)std::atoi(arg);
                            if (newH > 0)
                                this->heightWindow = newH;
                        }
                        break;
                        case EXPECTED_WIDTH_SCREEN:
                        {
                            auto newW = (unsigned int)std::atoi(arg);
                            if (newW > 0)
                            {
                                DYNAMIC_VAR * D_expectedW = this->device->lsDynamicVarGlobal["expectedwidth"];
                                if(D_expectedW == nullptr)
                                    this->device->lsDynamicVarGlobal["expectedwidth"] = new DYNAMIC_VAR(DYNAMIC_INT,&newW);
                                else if(D_expectedW->type == DYNAMIC_INT)
                                    D_expectedW->setInt(newW);
                                else if(D_expectedW->type == DYNAMIC_FLOAT)
                                    D_expectedW->setFloat(static_cast<float>(newW));
                                else
                                {
                                    delete D_expectedW;
                                    this->device->lsDynamicVarGlobal["expectedwidth"] = new DYNAMIC_VAR(DYNAMIC_INT,&newW);
                                }
                            }
                        }
                        break;
                        case EXPECTED_HEIGHT_SCREEN:
                        {
                            auto newH = (unsigned int)std::atoi(arg);
                            if (newH > 0)
                            {
                                DYNAMIC_VAR * D_expectedH = this->device->lsDynamicVarGlobal["expectedheight"];
                                if(D_expectedH == nullptr)
                                    this->device->lsDynamicVarGlobal["expectedheight"] = new DYNAMIC_VAR(DYNAMIC_INT,&newH);
                                else if(D_expectedH->type == DYNAMIC_INT)
                                    D_expectedH->setInt(newH);
                                else if(D_expectedH->type == DYNAMIC_FLOAT)
                                    D_expectedH->setFloat(static_cast<float>(newH));
                                else
                                {
                                    delete D_expectedH;
                                    this->device->lsDynamicVarGlobal["expectedheight"] = new DYNAMIC_VAR(DYNAMIC_INT,&newH);
                                }
                            }
                        }
                        break;
                        case POSITION_X_SCREEN: { this->positionXWindow = (unsigned int)std::atoi(arg);}
                        break;
                        case POSITION_Y_SCREEN: { this->positionYWindow = (unsigned int)std::atoi(arg);}
                        break;
                        case MAXIMIZED_WINDOW: { this->maximizedWindow = std::atoi(arg) ? true : false;}
                        break;
                        case INITIAL_SCENE_LUA: { this->fileNameInitialLua = argv[i];}
                        break;
                        case NAME_APP: 
                        { 
                            this->nameAplication = argv[i];
                        }
                        break;
                        case ADD_PATH: { util::addPath(arg);}
                        break;
                        case EXECUTE_STRING:
                        {
                            nextArg = NONE;
                        }
                        break;
                        default:
                        {
                            nextArg = NONE;
                            std::vector<std::string> result;
                            util::split(result, arg, '.');
                            if (result.size() >= 2 && result[result.size() - 1].compare("lua") == 0 &&
                                (this->fileNameInitialLua.compare("main.lua") == 0 || this->fileNameInitialLua.size() == 0))
                            {
                                this->fileNameInitialLua = argv[i];
                            }
                        }
                        break;
                    }
                    if (nextArg != NONE)
                        lastArg = nextArg;
                    nextArg     = NONE;
                }
            }
            if(this->fileNameInitialLua.size() == 0)
            {
                if(device->verbose)
                    WARN_LOG("\nScene not set... setting it to main.lua");
                this->fileNameInitialLua = "main.lua";
            }
        }

    int SCENE_SCRIPT::onNewScene(lua_State *lua)//same as mbm.loadScene
    {
        const int    top        = lua_gettop(lua);
        DEVICE *device			= DEVICE::getInstance();
        auto *luaManager = static_cast<LUA_MANAGER *>(device->ptrManager);
        if (luaManager && device)
        {
            const char *  nameScene = luaL_checkstring(lua, 1);
			const int tSplash       = top > 1 ? lua_type(lua,2) : LUA_TNONE;
			nameScene               = util::getFullPath(nameScene,nullptr);
			if (luaManager->__sceneWasInit == false)
            {
				if(luaManager->device->verbose)
                lua_print_line(lua,TYPE_LOG_WARN,"The scene [%s] will be load in the main loop!", nameScene);
                std::string nameSceneTmp(nameScene);
                log_util::replaceString(nameSceneTmp,"\\\\","/");
				log_util::replaceString(nameSceneTmp,"\\","/");
                auto *curScene          = static_cast<SCENE_SCRIPT*>(luaManager->device->scene);
                curScene->loadSceneOnFirtLoop        = true;
                curScene->strNameSceneLoadOnFirtLoop = std::move(nameSceneTmp);
                lua_pushboolean(lua, 1);
				return 1;
            }
            
			const std::string newSceneName(nameScene ? nameScene : "");
            if (device->scene && access_file(newSceneName.c_str(), 0) == 0)
            {
				auto *curScene          = static_cast<SCENE_SCRIPT*>(luaManager->device->scene);
                auto newScene = new SCENE_SCRIPT(newSceneName.c_str(), false, curScene->splashRenderizable);
                luaManager->lsScene.push_back(newScene);
                device->scene->nextScene     = newScene;
                device->scene->goToNextScene = true;
                device->scene->endScene      = true;

				if (tSplash == LUA_TTABLE)//add or replace renderizable
				{
					RENDERIZABLE *ptr        = getRenderizableFromRawTable(lua, 1, 2);
					newScene->setRenderizableLoading(ptr);
				}
				else if (tSplash == LUA_TSTRING)//add or replace renderizable as (texture view)
				{
					const char* file_name = lua_tostring(lua,2);
					mbm::TEXTURE_VIEW*  texture_view = new mbm::TEXTURE_VIEW(newScene,false,true);
                    if(newScene->splashRenderizable)
                        delete newScene->splashRenderizable;
                    newScene->splashRenderizable = nullptr;

					if(texture_view->load(file_name))
						newScene->splashRenderizable = texture_view;
					else
						delete texture_view;
				}
				else if (tSplash == LUA_TNIL)//remove renderizable
				{
					newScene->setRenderizableLoading(nullptr);
				}
				lua_pushboolean(lua, 1);
            }
            else if(luaManager->execute_string(lua) == false)//maybe there is a command string to execute
            {
                ERROR_LOG("Scene not found:[%s]", newSceneName.c_str());
                lua_pushboolean(lua, 0);
            }
        }
        else
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"luaManager not found");
            lua_pushboolean(lua, 0);
        }
        return 1;
    }

    bool LUA_MANAGER::execute_string(lua_State *lua)
    {
        if(string_to_execute.size()> 0)
        {
            std::string new_string(string_to_execute);
            string_to_execute.clear();
            if (luaL_dostring(lua, new_string.c_str()))
            {
                lua_print_line(lua,TYPE_LOG_ERROR,"\n%s", luaL_checkstring(lua, -1));
            }
            return true;
        }
        return false;
    }

    void SCENE_SCRIPT::loadMainScene(void *pLua_manager, const char *nameMainScene)
    {
        static bool loadedMainScene = false;
        if (loadedMainScene == false)
		{
			auto *luaManager = static_cast<LUA_MANAGER *>(pLua_manager);
			if (luaManager)
			{
				if (luaManager->lsScene.size())
				{
					if (luaManager->__sceneWasInit == false)
					{
                        if(device->verbose)
						    WARN_LOG("Scene [%s] cannot be loaded in the function", nameMainScene);
					}
                    else
                    {
                        const char *  nameScene = util::getFullPath(nameMainScene,nullptr);
                        if (access_file(nameScene, 0) == 0)
                        {
                            auto *curScene          = static_cast<SCENE_SCRIPT*>(luaManager->device->scene);
                            auto newScene = new SCENE_SCRIPT(nameScene, luaManager->noSplash,curScene->splashRenderizable);
                            luaManager->lsScene.push_back(newScene);
                            luaManager->device->scene->nextScene     = newScene;
                            luaManager->device->scene->goToNextScene = true;
                            luaManager->device->scene->endScene      = true;
                        }
                        else
                        {
                            ERROR_LOG("Scene not found:[%s]", nameScene);
                            luaManager->device->scene->endScene = true;
                        }
                    }
				}
                loadedMainScene = true;
			}
		}
    }

	void SCENE_SCRIPT::removePreviousSceneOnUnload()
    {
        DEVICE *device     = DEVICE::getInstance();
        auto *luaManager = static_cast<LUA_MANAGER *>(device->ptrManager);
        if (luaManager)
        {
            for (unsigned int i = 0; i < luaManager->lsScene.size(); ++i)
            {
                SCENE_SCRIPT *ptrScene = luaManager->lsScene[i];
                if (ptrScene != this)
                {
					luaManager->lsScene.erase(luaManager->lsScene.begin() + i);
                    delete ptrScene;
                }
            }
        }
    }

    bool SCENE_SCRIPT::logo_was_init = false;

	std::vector<std::string> LUA_MANAGER::globals_lua = {	"expectedwidth",
															"expectedheight",
															"stretch",
															"_executable_name_",
															"fileNameScene",
															"disable-coroutine-loading",
															"__debug"};

	const std::vector<std::string> get_globals_lua()
	{
		return LUA_MANAGER::globals_lua;
	}

	void LUA_MANAGER::onScriptPrintLine()
	{
		auto *luaManager = static_cast<LUA_MANAGER *>(LUA_MANAGER::pLuaManager);
		if(luaManager)
		{
			auto *curScene   = static_cast<SCENE_SCRIPT*>(luaManager->device->scene);
			if(curScene)
			{
				lua_print_line(curScene->getLuaState() ,TYPE_LOG_ERROR,"Attempt to get root cause...");
			}
		}
	}

	void LUA_MANAGER::onAddPathScript(const char * path)
	{
		auto *luaManager = static_cast<LUA_MANAGER *>(LUA_MANAGER::pLuaManager);
		if(luaManager)
		{
			auto *curScene   = static_cast<SCENE_SCRIPT*>(luaManager->device->scene);
			if(curScene)
			{
				lua_State* lua = curScene->getLuaState();
				
				const char * __addPathPackage =
			#if defined _WIN32
				
					R"__ADD_PACKAGE(
					local function __addPathPackage(path)
	
						local path_lua = path .. '\\\\?.lua'
						local p = package.path:split(';')
						for i=1, #p do
							local each_path = p[i]
							if each_path == path_lua then
								return false
							end
						end
						package.path = package.path .. ';' .. path_lua

	
						local path_dll = path .. '\\\\?.dll'
						local cp = package.cpath:split(';')
						for i=1, #cp do
							local each_path = cp[i]
							if each_path == path_dll then
								return false
							end
						end
						package.cpath = package.cpath .. ';' .. path_dll
						return true
					end
				)__ADD_PACKAGE";

				std::string win_path(path);
				log_util::replaceString(win_path,"\\\\","/");
				log_util::replaceString(win_path,"\\","/");
				log_util::replaceString(win_path,"/","\\\\");


				const int   len = win_path.size();
				if (len > 2)
				{
					const char *f = &win_path.c_str()[len - 2];
					if (strncmp(f, "\\\\",2) == 0)
						win_path.resize(len - 2);
				}
				
				path = win_path.c_str();
			#else
				
				R"__ADD_PACKAGE(
					local function __addPathPackage(path)
	
						local path_lua = path .. '/?.lua'
						local p = package.path:split(';')
						for i=1, #p do
							local each_path = p[i]
							if each_path == path_lua then
								return false
							end
						end
						package.path = package.path .. ';' .. path_lua


						local path_so = path .. '/?.so'
						local cp = package.cpath:split(';')
						for i=1, #cp do
							local each_path = cp[i]
							if each_path == path_so then
								return false
							end
						end
						package.cpath = package.cpath .. ';' .. path_so
						return true
					end
				)__ADD_PACKAGE";

				std::string linux_path(path);

				const int   len = linux_path.size();
				if (len > 1)
				{
					const char *f = &linux_path.c_str()[len - 1];
					if (strncmp(f, "/",1) == 0)
					{
						linux_path.resize(len - 1);
						path = linux_path.c_str();
					}
				}
			#endif

				std::string exe_string(__addPathPackage);
				exe_string += "__addPathPackage('";
				exe_string += path;
				exe_string += "') ";
				
				if (luaL_dostring(lua, exe_string.c_str()))
				{
					lua_print_line(lua,TYPE_LOG_ERROR,"\n%s", luaL_checkstring(lua, -1));
				}
			}
		}
	}

    #if !defined ANDROID
    int onDoCommands(lua_State *lua)
    {
        const int   top  = lua_gettop(lua);
        const char *what = luaL_checkstring(lua, 1);
        const char *parameter = top > 1 ? luaL_checkstring(lua, 2) : "";
        auto *luaManager = static_cast<LUA_MANAGER *>(LUA_MANAGER::pLuaManager);
        char result[1024] = "";
        if(luaManager->onDoNativeCommand)
            luaManager->onDoNativeCommand(what,parameter,result,sizeof(result));
        lua_pushstring(lua,result);
        return 1;
    }
    #endif
};
    #ifdef _WIN32
        #pragma warning(pop)
    #endif
