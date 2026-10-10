#include "llviewerprecompiledheaders.h"
#include "alkithelp.h"
#include "alkitcard.h"
#include "alkitpage.h"
#include "alkitrow.h"
#include "alkitstyle.h"
#include "llfontgl.h"
#include "llpanel.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"

namespace
{
LLTextBox* makeText(LLPanel* parent,const std::string& name,S32 top,S32 height,bool wrap=false)
{
    LLTextBox::Params params;
    params.name=name;
    params.rect=LLRect(10,top,parent->getRect().getWidth()-10,top-height);
    params.wrap=wrap;
    params.font=LLFontGL::getFontSansSerif();
    LLTextBox* text=LLUICtrlFactory::create<LLTextBox>(params);
    parent->addChild(text);
    return text;
}
}

void ALKitHelp::attach(LLPanel* panel,LLPanel* strip)
{
    if(!panel||!strip)return;
    mPanel=panel->getHandle();
    mStrip=strip->getHandle();
    mKicker=makeText(panel,"kit_help_kicker",panel->getRect().getHeight()-10,18);
    mTitle=makeText(panel,"kit_help_title",panel->getRect().getHeight()-32,24);
    LLTextEditor::Params body_params;
    body_params.name="kit_help_body";
    body_params.rect=LLRect(10,panel->getRect().getHeight()-62,panel->getRect().getWidth()-10,20);
    body_params.read_only=true;
    body_params.wrap=true;
    body_params.show_context_menu=false;
    mBody=LLUICtrlFactory::create<LLTextEditor>(body_params);
    panel->addChild(mBody);
    mMeta=makeText(panel,"kit_help_meta",panel->getRect().getHeight()-250,130,true);
    mStripText=makeText(strip,"kit_help_strip_text",24,22);
    mKicker->setFont(LLFontGL::getFontSansSerifSmall());
    mTitle->setFont(LLFontGL::getFontSansSerifBig());
    mMeta->setFont(LLFontGL::getFontSansSerifSmall());
}

void ALKitHelp::layout(S32 width,S32 height,S32 strip_width)
{
    const S32 text_width=llmax(1,width-20);
    if(mKicker){mKicker->reshape(text_width,18,true);mKicker->setOrigin(10,height-28);}
    if(mTitle){mTitle->reshape(text_width,24,true);mTitle->setOrigin(10,height-54);}
    if(mBody){mBody->reshape(text_width,llmin(260,llmax(80,height-260)),true);mBody->setOrigin(10,height-64-mBody->getRect().getHeight());}
    if(mMeta){mMeta->reshape(text_width,150,true);mMeta->setOrigin(10,20);}
    if(mStripText){mStripText->reshape(llmax(1,strip_width-20),22,true);mStripText->setOrigin(10,1);}
}

void ALKitHelp::showRow(ALKitRow* row)
{
    if(!row)return;
    const ALKitRowMeta& meta=row->meta();
    std::string detail;
    if(!meta.default_value.empty())detail="Default "+meta.default_value;
    if(meta.has_min||meta.has_max)
    {
        if(!detail.empty())detail+="\n";
        detail+=(meta.kind=="dial"||meta.kind=="arc")
            ? "Range "+llformat("%.3g\xC2\xB0 - %.3g\xC2\xB0",meta.min_val,meta.max_val)
            : "Range "+llformat("%.3g - %.3g",meta.min_val,meta.max_val);
    }
    if(!meta.unit.empty())detail+="\nUnit "+meta.unit;
    if(!meta.setting.empty())detail+="\nSetting "+meta.setting;
    if(!meta.widget.empty())detail+="\nWidget "+meta.widget;
    if(meta.advanced)detail+="\nIn More options";
    show(meta.page_key,meta.label,meta.help,detail);
}

void ALKitHelp::showCard(ALKitCard* card)
{
    if(card)show("",card->title(),card->helpText(),"");
}

void ALKitHelp::showPage(ALKitPage* page)
{
    if(page)show(page->pageKey(),page->pageKey(),"","");
}

void ALKitHelp::show(const std::string& kicker,const std::string& title,const std::string& body,const std::string& meta)
{
    if(mKicker)mKicker->setText(kicker);
    if(mTitle)mTitle->setText(title);
    if(mBody)mBody->setText(body);
    if(mMeta)mMeta->setText(meta+"\n\nRight-click a cast member for subject and mark actions.");
    if(mStripText)mStripText->setText(title+(body.empty()?std::string():" - "+body));
}
