#ifndef AL_KIT_HELP_H
#define AL_KIT_HELP_H

#include "llhandle.h"

#include <string>

class ALKitRow;
class ALKitCard;
class ALKitPage;
class LLPanel;
class LLTextBox;
class LLTextEditor;

class ALKitHelp
{
public:
    void attach(LLPanel* panel,LLPanel* strip);
    void showRow(ALKitRow*);
    void showCard(ALKitCard*);
    void showPage(ALKitPage*);
    void layout(S32 width,S32 height,S32 strip_width);
private:
    void show(const std::string&,const std::string&,const std::string&,const std::string&);
    LLHandle<LLPanel>mPanel;
    LLHandle<LLPanel>mStrip;
    LLTextBox*mKicker=nullptr;
    LLTextBox*mTitle=nullptr;
    LLTextEditor*mBody=nullptr;
    LLTextBox*mMeta=nullptr;
    LLTextBox*mStripText=nullptr;
};
#endif
