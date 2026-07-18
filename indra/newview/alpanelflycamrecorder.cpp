/**
 * @file alpanelflycamrecorder.cpp
 * @brief Flycam Recorder transport panel -- see alpanelflycamrecorder.h.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelflycamrecorder.h"

#include "llbutton.h"
#include "llflycamrecorder.h"
#include "llsliderctrl.h"
#include "lltextbox.h"
#include "llviewermenufile.h"   // LLFilePickerReplyThread

// both the standalone Flycam Recorder floater and the Director Console Takes tab
// instantiate this class via <panel class="panel_flycam_recorder" .../>. The
// injector string MUST match the class= string in the XML or the panel silently
// falls back to a plain LLPanel and none of the wiring below runs.
static LLPanelInjector<ALPanelFlycamRecorder> t_panel_flycam_recorder("panel_flycam_recorder");

namespace
{
// take file I/O routes through the recorder singleton, so a host (floater or
// console) closing while the picker is up can't dangle a dead 'this'
void pickerSave(const std::vector<std::string>& filenames,
                LLFilePicker::ELoadFilter, LLFilePicker::ESaveFilter)
{
    if (!filenames.empty())
    {
        LLFlycamRecorder::instance().saveToFile(filenames[0]);
    }
}

void pickerLoad(const std::vector<std::string>& filenames,
                LLFilePicker::ELoadFilter, LLFilePicker::ESaveFilter)
{
    if (!filenames.empty())
    {
        LLFlycamRecorder::instance().loadFromFile(filenames[0]);
    }
}
} // anonymous namespace

//static
void ALPanelFlycamRecorder::setToolTipIfChanged(LLUICtrl* ctrl, const std::string& tip)
{
    if (ctrl && ctrl->getToolTip() != tip)
    {
        ctrl->setToolTip(tip);
    }
}

bool ALPanelFlycamRecorder::postBuild()
{
    mRecordBtn  = getChild<LLButton>("btn_record");
    mPlayBtn    = getChild<LLButton>("btn_play");
    mStopBtn    = getChild<LLButton>("btn_stop");
    mClearBtn   = getChild<LLButton>("btn_clear");
    mSaveBtn    = getChild<LLButton>("btn_save");
    mLoadBtn    = getChild<LLButton>("btn_load");
    mScrub      = getChild<LLSliderCtrl>("scrub_slider");
    mTimeText   = getChild<LLTextBox>("time_lbl");
    mStatusText = getChild<LLTextBox>("status_lbl");

    mRecordBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onRecord(); });
    mPlayBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onPlayPause(); });
    mStopBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onStop(); });
    mClearBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClear(); });
    mSaveBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSave(); });
    mLoadBtn->setCommitCallback([this](LLUICtrl*, const LLSD&) { onLoad(); });
    mScrub->setCommitCallback([this](LLUICtrl*, const LLSD&) { onScrub(); });

    return true;
}

void ALPanelFlycamRecorder::draw()
{
    LLFlycamRecorder& rec = LLFlycamRecorder::instance();
    const LLFlycamRecorder::EState state = rec.getState();
    const F32 duration = rec.getDuration();
    const bool recording = (state == LLFlycamRecorder::STATE_RECORDING);
    const bool have_take = rec.getNumKeyframes() > 0;

    mRecordBtn->setLabel(recording ? LLStringExplicit("Stop rec")
                                   : LLStringExplicit("Record"));
    mPlayBtn->setLabel(state == LLFlycamRecorder::STATE_PLAYING
                           ? LLStringExplicit("Pause")
                           : LLStringExplicit("Play"));
    mPlayBtn->setEnabled(have_take && !recording);
    setToolTipIfChanged(mPlayBtn,
        !have_take ? std::string("Record or load a take first")
        : recording ? std::string("Stop recording first")
                    : std::string("Play or pause the recorded take (takes over the camera)"));

    mScrub->setEnabled(have_take && !recording);
    setToolTipIfChanged(mScrub,
        !have_take ? std::string("Record or load a take first")
        : recording ? std::string("Stop recording first")
                    : std::string("Scrub through the take. Dragging while stopped previews that moment"));

    mScrub->setMaxValue(llmax(duration, 0.01f));
    mScrub->setValue(rec.getPlayhead());

    const F32 shown = recording ? duration : rec.getPlayhead();
    mTimeText->setText(llformat("%.1f / %.1f s   %d keys",
                                shown, duration, rec.getNumKeyframes()));
    mStatusText->setText(rec.getStatus());

    // Clear / Save need a take; Load is always available (all drive the one
    // singleton the standalone floater and the console Takes tab share)
    mClearBtn->setEnabled(have_take && !recording);
    setToolTipIfChanged(mClearBtn,
        !have_take ? std::string("No take to clear")
        : recording ? std::string("Stop recording first")
                    : std::string("Discard the current take"));
    mSaveBtn->setEnabled(have_take && !recording);
    setToolTipIfChanged(mSaveBtn,
        !have_take ? std::string("Record or load a take first")
        : recording ? std::string("Stop recording first")
                    : std::string("Save the take to a hand-editable XML file"));
    mLoadBtn->setEnabled(!recording);
    setToolTipIfChanged(mLoadBtn,
        recording ? std::string("Stop recording first")
                  : std::string("Load a take from an XML file"));

    LLPanel::draw();
}

void ALPanelFlycamRecorder::onRecord()
{
    LLFlycamRecorder& rec = LLFlycamRecorder::instance();
    if (rec.getState() == LLFlycamRecorder::STATE_RECORDING)
    {
        rec.stopRecording();
    }
    else
    {
        rec.startRecording();
    }
}

void ALPanelFlycamRecorder::onPlayPause()
{
    LLFlycamRecorder::instance().togglePlayback();
}

void ALPanelFlycamRecorder::onStop()
{
    LLFlycamRecorder& rec = LLFlycamRecorder::instance();
    if (rec.getState() == LLFlycamRecorder::STATE_RECORDING)
    {
        rec.stopRecording();
    }
    else
    {
        rec.stopPlayback();
    }
}

void ALPanelFlycamRecorder::onClear()
{
    LLFlycamRecorder::instance().clear();
}

void ALPanelFlycamRecorder::onSave()
{
    // callbacks go through the recorder singleton, so an early host close
    // while the picker is up can't dangle
    LLFilePickerReplyThread::startPicker(&pickerSave, LLFilePicker::FFSAVE_XML,
                                         "flycam_take.xml");
}

void ALPanelFlycamRecorder::onLoad()
{
    LLFilePickerReplyThread::startPicker(&pickerLoad, LLFilePicker::FFLOAD_XML, false);
}

void ALPanelFlycamRecorder::onScrub()
{
    // commit only fires on user interaction (draw()'s setValue doesn't),
    // so this is always a deliberate scrub; from idle it previews the pose
    LLFlycamRecorder::instance().seek(mScrub->getValueF32());
}
