#ifndef AL_LIGHT_RIG_MAP_H
#define AL_LIGHT_RIG_MAP_H
#include "lluictrl.h"
#include "alkitmath.h"
#include <boost/signals2.hpp>
#include <array>
#include <string>
#include <vector>
class ALLightRigMap:public LLUICtrl
{
public:
 struct Params:public LLInitParam::Block<Params,LLUICtrl::Params>{Optional<std::string>selector_card,view_setting,side_from_setting;Params();};
 bool postBuild()override;void draw()override;bool handleMouseDown(S32,S32,MASK)override;bool handleMouseUp(S32,S32,MASK)override;bool handleHover(S32,S32,MASK)override;bool handleScrollWheel(S32,S32,LLScrollDelta)override{return false;}bool handleKeyHere(KEY,MASK)override;bool handleUnicodeCharHere(llwchar)override;S32 heightForWidth(S32 width)const;void setViewMode(S32 view);
protected:friend class LLUICtrlFactory;explicit ALLightRigMap(const Params&);
private:
 void refreshSetting(S32 role);void refreshLegend(S32 role);void refreshLiveReadouts();void beginDrag(S32 role,S32 x,S32 y);void applyDrag(S32 x,S32 y);void select(S32 role);void cancelDrag();
 std::string mSelectorCard,mViewSetting,mSideSetting;std::array<std::string,4>mYawNames,mPitchNames,mOnNames,mOffsetNames;std::array<F32,4>mYaw{},mPitch{},mOffset{};std::array<bool,4>mOn{};std::array<F32,4>mScreenX{},mScreenY{},mHitRadius{};std::vector<boost::signals2::scoped_connection>mConnections;
 std::array<std::string,4>mLegendText;std::string mFooterText;std::array<S32,4>mDrawOrder{{0,1,2,3}};
 S32 mView=2,mSideFrom=0,mSelected=0,mDragRole=-1;F32 mCameraBearing=0.f,mCanvasRadius=1.f,mCanvasCx=0.f,mCanvasCy=0.f,mDragDisplayYaw=0.f,mDragDisplayPitch=0.f,mRigRadius=1.5f,mOrbitYaw=0.f,mOrbitPitch=0.f;bool mMirror=false,mLiveTarget=false,mDragWrote=false,mWriting=false;ALKitMath::DragStart mDrag;
};
#endif
