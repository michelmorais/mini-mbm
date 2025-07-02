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

#include <scene.h>
#include <device.h>


namespace mbm
{
    
    CONTROL_SCENE::CONTROL_SCENE() noexcept
    {
        this->idScene = getNextIdScene();
    }
    CONTROL_SCENE::~CONTROL_SCENE() noexcept = default;

    int CONTROL_SCENE::getNextIdScene() noexcept
    {
        return idControl++;
    }
    int CONTROL_SCENE::getIdScene() const noexcept
    {
        return idScene;
    }

	DEVICE * COMMON_DEVICE::device = nullptr;

    COMMON_DEVICE::COMMON_DEVICE()
    {
        COMMON_DEVICE::device = DEVICE::getInstance();
    }
    
    
    SCENE::SCENE() noexcept
    {
        this->nextScene        = nullptr;
        this->goToNextScene    = true;
        this->endScene         = false;
        this->wasUnloadedScene = false;
        this->userData         = nullptr;
    }
    
    void * SCENE::get_lua_state()//if we are using lua we should be able to retrieve the current state
    {
        return nullptr;
    }
    
    const char * SCENE::getSceneName() noexcept
    {
        return nullptr;
    }
    
    void SCENE::onRestore(const int /*percent*/) // OnRestore da cena
    {
    }
    
    void SCENE::onTouchDown(int, float, float) // parameter: int key, float x, float y. Evento ao tocar na tela (click do
                                                // mouse. key = 0 botão esquerdo do mouse, key = 1 botão direito do mouse,
                                                // key = 2 botão do meio do mouse).
    {
    }
    
    void SCENE::onTouchUp(int, float, float) // parameter: int key, float x, float y. Evento ao soltar o toque da tela
                                              // (click do mouse. key = 0 botão esquerdo do mouse, key = 1 botão direito
                                              // do mouse, key = 2 botão do meio do mouse).
    {
    }
    
    void SCENE::onTouchMove(int, float, float) // parameter: float x, float y. Evento ao mover na tela (Ponteiro do
                                                // mouse)
    {
    }
    
    void SCENE::onTouchZoom(float) // Parameter float zoom de -1 a +1. Evento chamado ao solicitar zoom. Zoom estes
                                    // normalmente com movimentos dos dedos. É enviados valores entre -1 e +1. No caso de
                                    // mouse é o scrool do mesmo.
    {
    }
    
    void SCENE::onFinalizeScene() // method chamado ao encerrar a cena
    {
    }
    
    void SCENE::onKeyDown(int) // parameter: int key. Evento chamado ao pressionar uma tecla na janela ativa. key é um VK
                                // padrão da api do Windows.
    {
    }
    
    void SCENE::onKeyUp(int) // parameter: int key. Evento chamado ao pressionar uma tecla na janela ativa. key é um VK
                              // padrão da api do Windows.
    {
    }
    
    void SCENE::onKeyDownJoystick(int, int) // parameter: int player, int key
    {
    }
    
    void SCENE::onKeyUpJoystick(int, int) // parameter: int player, int key
    {
    }
    
    void SCENE::onMoveJoystick(int, float, float, float,
                                float) // parameter: int player, float lx, float ly, float rx, float ry
    {
    }
    
    void SCENE::onInfoDeviceJoystick(
        int, int, const char *,
        const char *) // parameter: int player, int maxNumberButton, const char* strDeviceName, const char* extraInfo
    {
    }
    
    void SCENE::setNextScene(SCENE *_nextScene)
    {
        this->nextScene = _nextScene;
    }
    
    void SCENE::onDoubleClick(float, float, int) // Double click do mouse. key ==0 botão esquerdo; key == 1 botão
                                                  // direito.
    {
    }
    
    void SCENE::onCallBackCommands(const char *,
                       const char *) // Callback de comandos nativos: (const char* functionNameCallBack,const char*param)
    {
    }

    int                    mbm::CONTROL_SCENE::idControl                 = 0;
}
