/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2018 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                            |
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

/*
    Vigenère algorithm implementation (Just for fun)
*/

#include <vigenere.h>
#include <cctype>

bool my_isalpha(char ch)
{
    return std::isalpha(static_cast<uint8_t>(ch));
}

bool my_isupper(char ch)
{
    return std::isupper(static_cast<uint8_t>(ch));
}

namespace mbm
{
    VIGENERE::VIGENERE(const std::string & mykey)
    {
        const std::size_t len = mykey.size();
        for (std::size_t i = 0; i < len; i++) 
        {
            const char c = mykey[i];
            if (my_isalpha(c)) 
            {
                char const cc = my_isupper(c) ? 'A' : 'a';
                key.push_back(c - cc);
            }
        }
    }

    void VIGENERE::encrypt(const std::string & msg,std::string & encrypted_out)const
    {
        encrypted_out = do_encrypt_decrypt(msg,1);
    }

    void VIGENERE::decrypt(const std::string & msg, std::string & decrypted_out)const
    {
        decrypted_out = do_encrypt_decrypt(msg,-1);
    }

    std::string VIGENERE::do_encrypt_decrypt(const std::string & msg,const char sign)const
    {
        constexpr int maxLetters  = 26;
        const std::size_t len       = msg.size();
        const std::size_t keylen    = key.size();
        std::string out(len,'\0');
        std::size_t index = 0;
        for (std::size_t i = 0; i < len; i++) 
        {
            const char c = msg[i];
            if (my_isalpha(c))
            {
                char const cc = (my_isupper(c)) ? 'A' : 'a';
                out[i] = ((c + (key[index] * sign) - cc + maxLetters) % maxLetters) + cc;
            }
            else
            {
                out[i] = c;
            }
            index = (index+1) % keylen;
        }
        return out;
    }

}
