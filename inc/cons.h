#pragma once

#include <switch.h>
#include <string>
#include <vector>

#include "gfx.h"

class console
{
    public:
        console(int maxLines) { maxl = maxLines; lines.push_back(""); }
        void out(const std::string& s)
        {
            mutexLock(&consMutex);
            lines[cLine] += s;
            mutexUnlock(&consMutex);
        }

        void nl()
        {
            mutexLock(&consMutex);
            lines.push_back("");
            if((int)lines.size() == maxl)
            {
                lines.erase(lines.begin());
            }
            cLine = lines.size() - 1;
            mutexUnlock(&consMutex);
        }

        void draw(const font *f)
        {
            mutexLock(&consMutex);
            for(unsigned i = 0; i < lines.size(); i++)
                drawText(lines[i].c_str(), frameBuffer, f, 56, 92 + (i * 28), 18, clrCreateU32(0xFFFFFFFF));
            mutexUnlock(&consMutex);
        }
        
        void clear() //force clear the console
        {
            int total = (cLine + 1);
            for(int i = 0; i < total; i++)
            {
                if (cLine >= 1)
                {
                    lines.erase(lines.begin());
                    cLine = lines.size() - 1;
                }
            }
            mutexUnlock(&consMutex);
        }

    private:
        //jic
        Mutex consMutex = 0;
        std::vector<std::string> lines;
        int maxl, cLine = 0;
};