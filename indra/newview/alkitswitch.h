#ifndef AL_KIT_SWITCH_H
#define AL_KIT_SWITCH_H
#include "llcheckboxctrl.h"
#include <boost/signals2.hpp>

class ALKitSwitch : public LLCheckBoxCtrl
{
public:
    struct Params : public LLInitParam::Block<Params, LLCheckBoxCtrl::Params> { Params() = default; };
    using flip_signal_t = boost::signals2::signal<void(bool)>;
    void reshape(S32 width, S32 height, bool called_from_parent=true) override;
    void draw() override;
    void setValue(const LLSD& value) override;
    void setControlName(const std::string& name, LLView* context) override;
    void onCommit() override;
    bool isOn() const { return mButton->getToggleState(); }
    void writeBoundValue(bool value);
    boost::signals2::connection setFlipCallback(const flip_signal_t::slot_type& slot) { return mFlipSignal.connect(slot); }
protected:
    friend class LLUICtrlFactory;
    explicit ALKitSwitch(const Params& p);
private:
    void noteState(bool value);
    bool mLastState=false;
    bool mHaveState=false;
    flip_signal_t mFlipSignal;
    boost::signals2::scoped_connection mControlConnection;
};
#endif
