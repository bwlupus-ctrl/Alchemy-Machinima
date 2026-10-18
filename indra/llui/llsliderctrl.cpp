/**
 * @file llsliderctrl.cpp
 * @brief LLSliderCtrl base class
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

// [BDMerge C8] See llsliderctrl.h / llslider.h for the slider precision
// override note. donor: Black Dragon (NiranV Dean) 94c47d42de.

#include "linden_common.h"

#include "llsliderctrl.h"

#include "llmath.h"
#include "llfontgl.h"
#include "llgl.h"
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "llslider.h"
#include "llsliderwheel.h" // [SliderWheel]
#include "llstring.h"
#include "lltextbox.h"
#include "llui.h"
#include "lluiconstants.h"
#include "llcontrol.h"
#include "llfocusmgr.h"
#include "llresmgr.h"
#include "lluictrlfactory.h"

static LLDefaultChildRegistry::Register<LLSliderCtrl> r("slider");

LLSliderCtrl::LLSliderCtrl(const LLSliderCtrl::Params& p)
:   LLF32UICtrl(p),
    mLabelBox( NULL ),
    mEditor( NULL ),
    mTextBox( NULL ),
    mFont(p.font),
    mShowText(p.show_text),
    mCanEditText(p.can_edit_text),
    mVolumeText(p.volume_text),
    mPrecision(p.decimal_digits),
    mTextEnabledColor(p.text_color()),
    mTextDisabledColor(p.text_disabled_color()),
    mLabelWidth(p.label_width),
    mEditorCommitSignal(NULL)
{
    S32 top = getRect().getHeight();
    S32 bottom = 0;
    S32 left = 0;

    S32 label_width = p.label_width;
    S32 text_width = p.text_width;

    // Label
    if( !p.label().empty() )
    {
        if (!p.label_width.isProvided())
        {
            label_width = p.font()->getWidth(p.label);
        }
        LLRect label_rect( left, top, label_width, bottom );
        LLTextBox::Params params(p.slider_label);
        if (!params.rect.isProvided())
        {
            params.rect = label_rect;
        }
        if (!params.font.isProvided())
        {
            params.font = p.font;
        }
        params.initial_value(p.label());
        mLabelBox = LLUICtrlFactory::create<LLTextBox> (params);
        addChild(mLabelBox);
        mLabelFont = params.font();
    }

    if (p.show_text && !p.text_width.isProvided())
    {
        // calculate the size of the text box (log max_value is number of digits - 1 so plus 1)
        if ( p.max_value )
            text_width = p.font()->getWidth(std::string("0")) * ( static_cast < S32 > ( log10  ( p.max_value ) ) + p.decimal_digits + 1 );

        if ( p.increment < 1.0f )
            text_width += p.font()->getWidth(std::string(".")); // (mostly) take account of decimal point in value

        if ( p.min_value < 0.0f || p.max_value < 0.0f )
            text_width += p.font()->getWidth(std::string("-")); // (mostly) take account of minus sign

        // padding to make things look nicer
        text_width += 8;
    }


    S32 text_left = getRect().getWidth() - text_width;
    static LLUICachedControl<S32> sliderctrl_spacing ("UISliderctrlSpacing", 0);

    S32 slider_right = getRect().getWidth();
    if( p.show_text )
    {
        slider_right = text_left - sliderctrl_spacing;
    }

    S32 slider_left = label_width ? label_width + sliderctrl_spacing : 0;
    LLSlider::Params slider_p(p.slider_bar);
    slider_p.name("slider_bar");
    if (!slider_p.rect.isProvided())
    {
        slider_p.rect = LLRect(slider_left,top,slider_right,bottom);
    }
    if (!slider_p.initial_value.isProvided())
    {
        slider_p.initial_value = p.initial_value().asReal();
    }
    if (!slider_p.min_value.isProvided())
    {
        slider_p.min_value = p.min_value;
    }
    if (!slider_p.max_value.isProvided())
    {
        slider_p.max_value = p.max_value;
    }
    if (!slider_p.increment.isProvided())
    {
        slider_p.increment = p.increment;
    }
    if (!slider_p.orientation.isProvided())
    {
        slider_p.orientation = p.orientation;
    }
    // [SliderWheel] design 2.1: copy ONLY a wheel_adjust the ctrl was actually given. Copying the
    // ctrl's default (false) would mark the child provided = explicit OFF and defeat the global
    // default; an unprovided ctrl leaves the child UNSET.
    if (p.wheel_adjust.isProvided() && !slider_p.wheel_adjust.isProvided())
    {
        slider_p.wheel_adjust = p.wheel_adjust; // [AL] hover-wheel precision opt-in
    }
    if (!slider_p.wheel_fine_div.isProvided())
    {
        slider_p.wheel_fine_div = p.wheel_fine_div; // [EnvIntensity v2] Shift fine step opt-in
    }
    if (!slider_p.wheel_step.isProvided())
    {
        slider_p.wheel_step = p.wheel_step; // [SliderWheelStep] explicit per-notch step
    }

    slider_p.commit_callback.function = &LLSliderCtrl::onSliderCommit;
    slider_p.control_name = p.control_name;
    slider_p.mouse_down_callback( p.mouse_down_callback );
    slider_p.mouse_up_callback( p.mouse_up_callback );
    mSlider = LLUICtrlFactory::create<LLSlider> (slider_p);

    addChild( mSlider );

    if( p.show_text() )
    {
        LLRect text_rect( text_left, top, getRect().getWidth(), bottom );
        if( p.can_edit_text() )
        {
            LLLineEditor::Params line_p(p.value_editor);
            if (!line_p.rect.isProvided())
            {
                line_p.rect = text_rect;
            }
            if (!line_p.font.isProvided())
            {
                line_p.font = p.font;
            }

            line_p.commit_callback.function(&LLSliderCtrl::onEditorCommit);
            line_p.prevalidator(&LLTextValidate::validateFloat);
            mEditor = LLUICtrlFactory::create<LLLineEditor>(line_p);

            mEditor->setFocusReceivedCallback( boost::bind(&LLSliderCtrl::onEditorGainFocus, _1, this ));
            // don't do this, as selecting the entire text is single clicking in some cases
            // and double clicking in others
            //mEditor->setSelectAllonFocusReceived(true);
            addChild(mEditor);
        }
        else
        {
            LLTextBox::Params text_p(p.value_text);
            if (!text_p.rect.isProvided())
            {
                text_p.rect = text_rect;
            }
            if (!text_p.font.isProvided())
            {
                text_p.font = p.font;
            }
            mTextBox = LLUICtrlFactory::create<LLTextBox>(text_p);
            addChild(mTextBox);
        }
    }

    updateText();
}

LLSliderCtrl::~LLSliderCtrl()
{
    delete mEditorCommitSignal;
}

// static
void LLSliderCtrl::onEditorGainFocus( LLFocusableElement* caller, void *userdata )
{
    LLSliderCtrl* self = (LLSliderCtrl*) userdata;
    llassert( caller == self->mEditor );

    self->onFocusReceived();
}


// [SliderWheel] design v7 4.2b.
LLSlider* LLSliderCtrl::liveSlider(const LLHandle<LLSlider>& sh) const
{
    LLSlider* s = LLSliderWheel::alive(sh);
    if (s && s->getParent() == this && mSlider == s)
    {
        return s;
    }
    return nullptr;
}

// [SliderWheel] RS(): after every foreign call where execution continues, re-resolve the ctrl AND
// the child; return before any further access if either failed. Needs locals h, sh, self, s.
// review P1-2: when ctrl/child re-resolution fails, the ORIGINAL slider (which may still exist,
// removed / reparented / replaced / under a die()d ancestor) gets its pending wheel settle
// cancelled, so a detached slider can never fire a settle later.
static void cancelWheelSettleOf(const LLHandle<LLSlider>& sh)
{
    if (LLSlider* old = sh.get())
    {
        old->cancelWheelSettle();
    }
}

#define SLIDERWHEEL_RS() \
    do { \
        self = LLSliderWheel::alive(h); \
        if (!self) { cancelWheelSettleOf(sh); return; } \
        s = self->liveSlider(sh); \
        if (!s) { cancelWheelSettleOf(sh); return; } \
    } while (0)

void LLSliderCtrl::setValue(F32 v, bool from_event, bool precision_override)
{
    // [BDMerge C8]
    // [SliderWheel] the child's control write runs listeners (foreign code) that may destroy the
    // ctrl, only the child, or die() an ancestor: handles first, re-resolve both afterwards and
    // touch mSlider/mValue/updateText only through the re-resolved pointers, never `this`.
    LLHandle<LLSliderCtrl> h  = getDerivedHandle<LLSliderCtrl>();
    LLHandle<LLSlider>     sh = mSlider->getDerivedHandle<LLSlider>();
    mSlider->setValue( v, from_event, precision_override );
    LLSliderCtrl* self = LLSliderWheel::alive(h);
    if (!self) { cancelWheelSettleOf(sh); return; }
    LLSlider* s = self->liveSlider(sh);
    if (!s) { cancelWheelSettleOf(sh); return; }
    self->mValue = s->getValueF32();
    self->updateText();
}

// [EnvIntensity v2] Control-variable sync path. For a slider without
// wheel_fine_div this is exactly the old inline body (mSlider->setValue(v,
// true, false)); with it, LLSlider keeps the incoming fine value.
void LLSliderCtrl::setValue(const LLSD& value)
{
    // [SliderWheel] identical hardening to the F32 overload above.
    LLHandle<LLSliderCtrl> h  = getDerivedHandle<LLSliderCtrl>();
    LLHandle<LLSlider>     sh = mSlider->getDerivedHandle<LLSlider>();
    mSlider->setValue(value);
    LLSliderCtrl* self = LLSliderWheel::alive(h);
    if (!self) { cancelWheelSettleOf(sh); return; }
    LLSlider* s = self->liveSlider(sh);
    if (!s) { cancelWheelSettleOf(sh); return; }
    self->mValue = s->getValueF32();
    self->updateText();
}

// [EnvIntensity v2] Gated on wheel_fine_div so the shipping wheel_adjust
// floaters keep their current hit area (slider bar only).
// [SliderWheel] wheelEnabled() = the resolved vertical-wheel state (design 2.3).
bool LLSliderCtrl::handleScrollWheel(S32 x, S32 y, LLScrollDelta delta)
{
    if (mSlider && mSlider->wheelEnabled() && mSlider->getWheelFineDiv() > 0.f)
    {
        return mSlider->handleScrollWheel(x, y, delta);
    }
    return LLF32UICtrl::handleScrollWheel(x, y, delta);
}

bool LLSliderCtrl::setLabelArg( const std::string& key, const LLStringExplicit& text )
{
    bool res = false;
    if (mLabelBox)
    {
        res = mLabelBox->setTextArg(key, text);
        if (res && mLabelFont && mLabelWidth == 0)
        {
            S32 label_width = mLabelFont->getWidth(mLabelBox->getWText().c_str());
            LLRect rect = mLabelBox->getRect();
            S32 prev_right = rect.mRight;
            rect.mRight = rect.mLeft + label_width;
            mLabelBox->setRect(rect);

            S32 delta = rect.mRight - prev_right;
            rect = mSlider->getRect();
            S32 left = rect.mLeft + delta;
            static LLUICachedControl<S32> sliderctrl_spacing ("UISliderctrlSpacing", 0);
            left = llclamp(left, 0, rect.mRight - sliderctrl_spacing);
            rect.mLeft = left;
            mSlider->setRect(rect);
        }
    }
    return res;
}

void LLSliderCtrl::clear()
{
    setValue(0.0f);
    if( mEditor )
    {
        mEditor->setText( LLStringUtil::null );
    }
    if( mTextBox )
    {
        mTextBox->setText( LLStringUtil::null );
    }

}

void LLSliderCtrl::updateText()
{
    LL_PROFILE_ZONE_NAMED_CATEGORY_UI("LLSliderCtrl::updateText");
    if( mEditor || mTextBox )
    {
        LLLocale locale(LLLocale::USER_LOCALE);

        // Don't display very small negative values as -0.000
        F32 displayed_value = (F32)(floor(getValueF32() * pow(10.0, (F64)mPrecision) + 0.5) / pow(10.0, (F64)mPrecision));

        std::string format = llformat("%%.%df", mPrecision);
        std::string text = llformat(format.c_str(), displayed_value);
        if (!mEditor && mVolumeText)
            text = displayed_value <= 0.f ? "Muted" : llformat("%.0f%%", displayed_value * 100.f);
        if( mEditor )
        {
            // Setting editor text here to "" before using actual text is here because if text which
            // is set is the same as the one which is actually typed into lineeditor, LLLineEditor::setText()
            // will exit at it's beginning, so text for revert on escape won't be saved. (EXT-8536)
            mEditor->setText( LLStringUtil::null );
            mEditor->setText( text );
        }
        else
        {
            mTextBox->setText( text );
        }
    }
}

void LLSliderCtrl::updateSliderRect()
{
    S32 right = getRect().getWidth();
    S32 top = getRect().getHeight();
    S32 bottom = 0;
    S32 left = 0;
    static LLUICachedControl<S32> sliderctrl_spacing("UISliderctrlSpacing", 0);
    if (mEditor)
    {
        LLRect editor_rect = mEditor->getRect();
        S32 editor_width = editor_rect.getWidth();
        editor_rect.mRight = right;
        editor_rect.mLeft = right - editor_width;
        mEditor->setRect(editor_rect);

        right -= editor_width + sliderctrl_spacing;
    }
    if (mTextBox)
    {
        // Keep the value text pinned to the right edge, the same way the
        // editable variant above is; without this a resized slider leaves
        // the text floating at its construction-time offset mid-track.
        LLRect text_rect = mTextBox->getRect();
        const S32 text_width = text_rect.getWidth();
        text_rect.mRight = right;
        text_rect.mLeft = right - text_width;
        mTextBox->setRect(text_rect);

        right -= text_width + sliderctrl_spacing;
    }
    if (mLabelBox)
    {
        left += mLabelBox->getRect().getWidth() + sliderctrl_spacing;
    }

    mSlider->setRect(LLRect(left, top,right,bottom));
}

// static
void LLSliderCtrl::onEditorCommit( LLUICtrl* ctrl, const LLSD& userdata )
{
    LLSliderCtrl* self = dynamic_cast<LLSliderCtrl*>(ctrl->getParent());
    if (!self)
        return;

    // [SliderWheel] design v7 4.2b: handles for the ctrl AND its child BEFORE the first foreign
    // call (setValue's control write, the validate signal, onCommit, the editor signal). After
    // each one SLIDERWHEEL_RS() re-resolves both and returns if either is gone.
    LLHandle<LLSliderCtrl> h  = self->getDerivedHandle<LLSliderCtrl>();
    LLHandle<LLSlider>     sh = self->mSlider->getDerivedHandle<LLSlider>();
    LLSlider* s = self->mSlider;

    bool success = false;
    F32 val = self->mValue;
    F32 saved_val = self->mValue;

    std::string text = self->mEditor->getText();
    if( LLLineEditor::postvalidateFloat( text ) )
    {
        LLLocale locale(LLLocale::USER_LOCALE);
        val = (F32) atof( text.c_str() );
        if( s->getMinValue() <= val && val <= s->getMaxValue() )
        {
            // [BDMerge C8] Typed values may land between increments when gated on.
            static LLCachedControl<bool> sPrecisionOverride(*LLUI::getInstance()->mSettingGroups["config"], "BDMergeSliderPrecision", false);
            self->setValue( val, false, sPrecisionOverride );  // set the value temporarily so that the callback can retrieve it.
            SLIDERWHEEL_RS();                                  // control write ran listeners
            const bool valid = !self->mValidateSignal || (*(self->mValidateSignal))( self, val );
            SLIDERWHEEL_RS();                                  // validate signal
            if( valid )
            {
                success = true;
            }
        }
    }

    if( success )
    {
        // [SliderWheel] the typed value is the final commit: cancel a pending wheel settle FIRST,
        // before any callback (s is fresh from the RS() above).
        s->cancelWheelSettle();
        self->onCommit();
        SLIDERWHEEL_RS();                                      // commit signal + control write
        if (self->mEditorCommitSignal)
        {
            const F32 committed = self->getValueF32();         // child read: s is fresh
            (*(self->mEditorCommitSignal))(self, committed);
            SLIDERWHEEL_RS();
        }
    }
    else
    {
        if( self->getValueF32() != saved_val )                 // child read: nothing foreign ran since the last RS()/entry
        {
            self->setValue( saved_val );
            SLIDERWHEEL_RS();                                  // reject-path control write
        }
        self->reportInvalidData();                             // sound only, no foreign code
    }
    SLIDERWHEEL_RS();                                          // fresh, immediately before updateText
    self->updateText();                                        // reads getValueF32() -> mSlider
}

// static
void LLSliderCtrl::onSliderCommit( LLUICtrl* ctrl, const LLSD& userdata )
{
    LLSliderCtrl* self = dynamic_cast<LLSliderCtrl*>(ctrl->getParent());
    if (!self)
        return;

    // [SliderWheel] design v7 4.2b (see onEditorCommit): handles first, RS() after every foreign call.
    LLHandle<LLSliderCtrl> h  = self->getDerivedHandle<LLSliderCtrl>();
    LLHandle<LLSlider>     sh = self->mSlider->getDerivedHandle<LLSlider>();
    LLSlider* s = self->mSlider;

    bool success = false;
    F32 saved_val = self->mValue;
    F32 new_val = s->getValueF32();                            // before any callback

    self->mValue = new_val;  // set the value temporarily so that the callback can retrieve it.
    if( !self->mValidateSignal || (*(self->mValidateSignal))( self, new_val ) )
    {
        success = true;
    }
    SLIDERWHEEL_RS();                                          // validate signal

    if( success )
    {
        self->onCommit();
        SLIDERWHEEL_RS();                                      // commit signal + control write
    }
    else
    {
        if( self->mValue != saved_val )
        {
            self->setValue( saved_val );
            SLIDERWHEEL_RS();                                  // reject-path control write
        }
        self->reportInvalidData();
    }
    SLIDERWHEEL_RS();                                          // fresh, immediately before updateText
    self->updateText();
}

#undef SLIDERWHEEL_RS

void LLSliderCtrl::setEnabled(bool b)
{
    LLView::setEnabled( b );

    if( mLabelBox )
    {
        mLabelBox->setColor( b ? mTextEnabledColor : mTextDisabledColor );
    }

    mSlider->setEnabled( b );

    if( mEditor )
    {
        mEditor->setEnabled( b );
    }

    if( mTextBox )
    {
        mTextBox->setColor( b ? mTextEnabledColor : mTextDisabledColor );
    }
}


void LLSliderCtrl::setTentative(bool b)
{
    if( mEditor )
    {
        mEditor->setTentative(b);
    }
    LLF32UICtrl::setTentative(b);
}


void LLSliderCtrl::onCommit()
{
    setTentative(false);

    if( mEditor )
    {
        mEditor->setTentative(false);
    }

    setControlValue(getValueF32());
    LLF32UICtrl::onCommit();
}

void LLSliderCtrl::setRect(const LLRect& rect)
{
    LLF32UICtrl::setRect(rect);
    updateSliderRect();
}

//virtual
void LLSliderCtrl::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLF32UICtrl::reshape(width, height, called_from_parent);
    updateSliderRect();
}

void LLSliderCtrl::setPrecision(S32 precision)
{
    if (precision < 0 || precision > 10)
    {
        LL_ERRS() << "LLSliderCtrl::setPrecision - precision out of range" << LL_ENDL;
        return;
    }

    mPrecision = precision;
    updateText();
}

boost::signals2::connection LLSliderCtrl::setSliderMouseDownCallback( const commit_signal_t::slot_type& cb )
{
    return mSlider->setMouseDownCallback( cb );
}

boost::signals2::connection LLSliderCtrl::setSliderMouseUpCallback( const commit_signal_t::slot_type& cb )
{
    return mSlider->setMouseUpCallback( cb );
}

boost::signals2::connection LLSliderCtrl::setSliderEditorCommitCallback( const commit_signal_t::slot_type& cb )
{
    if (!mEditorCommitSignal) mEditorCommitSignal = new commit_signal_t();
    return mEditorCommitSignal->connect(cb);
}
void LLSliderCtrl::onTabInto()
{
    if( mEditor )
    {
        mEditor->onTabInto();
    }
    LLF32UICtrl::onTabInto();
}

void LLSliderCtrl::reportInvalidData()
{
    make_ui_sound("UISndBadKeystroke");
}
