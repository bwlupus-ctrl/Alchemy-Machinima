/**
 * @file alpanelatmosphericvolumes.cpp
 * @brief Shared editor for client-only atmospheric fog volumes.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "alpanelatmosphericvolumes.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llmath.h"
#include "m3math.h"
#include "llscrolllistctrl.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llstring.h"
#include "llviewercamera.h"

static LLPanelInjector<ALPanelAtmosphericVolumes>
    t_panel_atmospheric_volumes("panel_atmospheric_volumes");

namespace
{
bool valid_volume(S32 volume)
{
    return volume >= 0 && volume < ALLocalFogManager::MAX_VOLUMES;
}
} // anonymous namespace

ALPanelAtmosphericVolumes::ALPanelAtmosphericVolumes() = default;
ALPanelAtmosphericVolumes::~ALPanelAtmosphericVolumes() = default;

bool ALPanelAtmosphericVolumes::postBuild()
{
    mVolumeList = getChild<LLScrollListCtrl>("localfog_volume_list");
    mAddButton = getChild<LLButton>("localfog_add");
    mDuplicateButton = getChild<LLButton>("localfog_duplicate");
    mDeleteButton = getChild<LLButton>("localfog_delete");
    mMoveToCameraButton = getChild<LLButton>("localfog_move_camera");
    mEnabledCheck = getChild<LLCheckBoxCtrl>("localfog_enabled");
    mLabelEditor = getChild<LLLineEditor>("localfog_label");
    mCenter[VX] = getChild<LLSpinCtrl>("localfog_center_x");
    mCenter[VY] = getChild<LLSpinCtrl>("localfog_center_y");
    mCenter[VZ] = getChild<LLSpinCtrl>("localfog_center_z");
    mSize[VX] = getChild<LLSpinCtrl>("localfog_size_x");
    mSize[VY] = getChild<LLSpinCtrl>("localfog_size_y");
    mSize[VZ] = getChild<LLSpinCtrl>("localfog_size_z");
    mRotation[VX] = getChild<LLSpinCtrl>("localfog_rotation_x");
    mRotation[VY] = getChild<LLSpinCtrl>("localfog_rotation_y");
    mRotation[VZ] = getChild<LLSpinCtrl>("localfog_rotation_z");
    mShapeCombo = getChild<LLComboBox>("localfog_shape");
    mDensitySlider = getChild<LLSliderCtrl>("localfog_density");
    mFeatherSlider = getChild<LLSliderCtrl>("localfog_feather");
    mHeightSlider = getChild<LLSliderCtrl>("localfog_height_falloff");
    mNoiseScaleSlider = getChild<LLSliderCtrl>("localfog_noise_scale");
    mNoiseSpeedSlider = getChild<LLSliderCtrl>("localfog_noise_speed");
    mTintSwatch = getChild<LLColorSwatchCtrl>("localfog_tint");

    mVolumeList->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onSelectionChanged(); });
    mAddButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onAdd(); });
    mDuplicateButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onDuplicate(); });
    mDeleteButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onDelete(); });
    mMoveToCameraButton->setCommitCallback(
        [this](LLUICtrl*, const LLSD&) { onMoveToCamera(); });

    const auto editor_commit = [this](LLUICtrl*, const LLSD&) { onEditorCommit(); };
    mEnabledCheck->setCommitCallback(editor_commit);
    mLabelEditor->setCommitCallback(editor_commit);
    for (S32 axis = 0; axis < 3; ++axis)
    {
        mCenter[axis]->setCommitCallback(editor_commit);
        mSize[axis]->setCommitCallback(editor_commit);
        mRotation[axis]->setCommitCallback(editor_commit);
    }
    mShapeCombo->setCommitCallback(editor_commit);
    mDensitySlider->setCommitCallback(editor_commit);
    mFeatherSlider->setCommitCallback(editor_commit);
    mHeightSlider->setCommitCallback(editor_commit);
    mNoiseScaleSlider->setCommitCallback(editor_commit);
    mNoiseSpeedSlider->setCommitCallback(editor_commit);
    mTintSwatch->setCommitCallback(editor_commit);
    mTintSwatch->setCanApplyImmediately(true);

    mSelected = ALLocalFogManager::instance().selectedVolume();
    refreshBank(true);
    return true;
}

void ALPanelAtmosphericVolumes::draw()
{
    refreshBank(false);
    const S32 manager_selection =
        ALLocalFogManager::instance().selectedVolume();
    if (manager_selection != mSelected && valid_volume(manager_selection))
    {
        mSelected = manager_selection;
        mVolumeList->selectByValue(LLSD(mSelected));
        refreshEditor();
    }
    LLPanel::draw();
}

//static
std::string ALPanelAtmosphericVolumes::displayLabel(
    const Volume& volume, S32 index)
{
    if (!volume.mLabel.empty())
    {
        return volume.mLabel;
    }
    return llformat("Slot %d (empty)", index + 1);
}

void ALPanelAtmosphericVolumes::onSelectionChanged()
{
    if (mRefreshing)
    {
        return;
    }
    const S32 selected = mVolumeList->getSelectedValue().asInteger();
    if (!valid_volume(selected))
    {
        return;
    }
    mSelected = selected;
    ALLocalFogManager::instance().setSelected(selected);
    refreshEditor();
}

void ALPanelAtmosphericVolumes::onAdd()
{
    ALLocalFogManager::Bank& bank = ALLocalFogManager::instance().volumes();
    for (S32 index = 0; index < (S32)bank.size(); ++index)
    {
        if (bank[index].mLabel.empty() && !bank[index].mEnabled)
        {
            Volume volume;
            volume.mEnabled = true;
            volume.mLabel = llformat("Fog %d", index + 1);
            volume.mCenter = LLViewerCamera::getInstance()->getOrigin();
            bank[index] = volume;
            mSelected = index;
            ALLocalFogManager::instance().setSelected(index);
            saveBank();
            return;
        }
    }
}

void ALPanelAtmosphericVolumes::onDuplicate()
{
    ALLocalFogManager::Bank& bank = ALLocalFogManager::instance().volumes();
    if (!valid_volume(mSelected) ||
        bank.size() != (size_t)ALLocalFogManager::MAX_VOLUMES)
    {
        return;
    }
    for (S32 index = 0; index < (S32)bank.size(); ++index)
    {
        if (bank[index].mLabel.empty() && !bank[index].mEnabled)
        {
            bank[index] = bank[mSelected];
            bank[index].mEnabled = true;
            bank[index].mLabel = displayLabel(bank[mSelected], mSelected) + " copy";
            bank[index].mCenter.mV[VX] += 1.f;
            mSelected = index;
            ALLocalFogManager::instance().setSelected(index);
            saveBank();
            return;
        }
    }
}

void ALPanelAtmosphericVolumes::onDelete()
{
    ALLocalFogManager::Bank& bank = ALLocalFogManager::instance().volumes();
    if (!valid_volume(mSelected) ||
        bank.size() != (size_t)ALLocalFogManager::MAX_VOLUMES)
    {
        return;
    }
    bank[mSelected] = Volume();
    saveBank();
}

void ALPanelAtmosphericVolumes::onMoveToCamera()
{
    ALLocalFogManager::Bank& bank = ALLocalFogManager::instance().volumes();
    if (!valid_volume(mSelected) ||
        bank.size() != (size_t)ALLocalFogManager::MAX_VOLUMES)
    {
        return;
    }
    bank[mSelected].mCenter = LLViewerCamera::getInstance()->getOrigin();
    saveBank();
}

void ALPanelAtmosphericVolumes::onEditorCommit()
{
    ALLocalFogManager::Bank& bank = ALLocalFogManager::instance().volumes();
    if (mRefreshing || !valid_volume(mSelected) ||
        bank.size() != (size_t)ALLocalFogManager::MAX_VOLUMES)
    {
        return;
    }

    Volume& volume = bank[mSelected];
    volume.mEnabled = mEnabledCheck->getValue().asBoolean();
    volume.mLabel = mLabelEditor->getText();
    for (S32 axis = 0; axis < 3; ++axis)
    {
        volume.mCenter.mV[axis] = (F32)mCenter[axis]->getValue().asReal();
        volume.mSize.mV[axis] = (F32)mSize[axis]->getValue().asReal();
    }
    const F32 roll = (F32)mRotation[VX]->getValue().asReal() * DEG_TO_RAD;
    const F32 pitch = (F32)mRotation[VY]->getValue().asReal() * DEG_TO_RAD;
    const F32 yaw = (F32)mRotation[VZ]->getValue().asReal() * DEG_TO_RAD;
    volume.mRotation = LLQuaternion(LLMatrix3(roll, pitch, yaw));
    volume.mShape = mShapeCombo->getValue().asInteger();
    volume.mDensity = (F32)mDensitySlider->getValue().asReal();
    volume.mFeather = (F32)mFeatherSlider->getValue().asReal();
    volume.mHeightFalloff = (F32)mHeightSlider->getValue().asReal();
    volume.mNoiseScale = (F32)mNoiseScaleSlider->getValue().asReal();
    volume.mNoiseSpeed = (F32)mNoiseSpeedSlider->getValue().asReal();
    const LLColor4 tint = mTintSwatch->get();
    volume.mColor = LLColor3(tint);
    saveBank();
}

void ALPanelAtmosphericVolumes::saveBank()
{
    ALLocalFogManager& manager = ALLocalFogManager::instance();
    manager.saveBank();
    mSeenRevision = manager.revision();
    refreshList();
    refreshEditor();
}

void ALPanelAtmosphericVolumes::refreshBank(bool force)
{
    const U32 revision = ALLocalFogManager::instance().revision();
    if (!force && revision == mSeenRevision)
    {
        return;
    }
    mSeenRevision = revision;
    if (!valid_volume(mSelected))
    {
        mSelected = 0;
    }
    refreshList();
    refreshEditor();
}

void ALPanelAtmosphericVolumes::refreshList()
{
    const ALLocalFogManager::Bank& bank =
        ALLocalFogManager::instance().volumes();
    mRefreshing = true;
    mVolumeList->deleteAllItems();
    for (S32 index = 0; index < (S32)bank.size(); ++index)
    {
        const Volume& volume = bank[index];
        LLSD row;
        row["value"] = index;
        row["columns"][0]["column"] = "label";
        row["columns"][0]["value"] = displayLabel(volume, index);
        row["columns"][1]["column"] = "enabled";
        row["columns"][1]["value"] = volume.mEnabled ? "On" : "Off";
        row["columns"][2]["column"] = "shape";
        row["columns"][2]["value"] =
            volume.mShape == ALLocalFogManager::SHAPE_ELLIPSOID
                ? "Ellipsoid" : "Box";
        mVolumeList->addElement(row, ADD_BOTTOM);
    }
    mVolumeList->selectByValue(LLSD(mSelected));
    mRefreshing = false;
}

void ALPanelAtmosphericVolumes::refreshEditor()
{
    const ALLocalFogManager::Bank& bank =
        ALLocalFogManager::instance().volumes();
    if (!valid_volume(mSelected) ||
        bank.size() != (size_t)ALLocalFogManager::MAX_VOLUMES)
    {
        return;
    }
    const Volume& volume = bank[mSelected];
    mRefreshing = true;
    mEnabledCheck->setValue(volume.mEnabled);
    mLabelEditor->setText(LLStringExplicit(volume.mLabel));
    for (S32 axis = 0; axis < 3; ++axis)
    {
        mCenter[axis]->setValue(LLSD((F64)volume.mCenter.mV[axis]));
        mSize[axis]->setValue(LLSD((F64)volume.mSize.mV[axis]));
    }
    F32 roll = 0.f;
    F32 pitch = 0.f;
    F32 yaw = 0.f;
    volume.mRotation.getEulerAngles(&roll, &pitch, &yaw);
    mRotation[VX]->setValue(LLSD((F64)(roll * RAD_TO_DEG)));
    mRotation[VY]->setValue(LLSD((F64)(pitch * RAD_TO_DEG)));
    mRotation[VZ]->setValue(LLSD((F64)(yaw * RAD_TO_DEG)));
    mShapeCombo->setValue(LLSD(volume.mShape));
    mDensitySlider->setValue(LLSD((F64)volume.mDensity));
    mFeatherSlider->setValue(LLSD((F64)volume.mFeather));
    mHeightSlider->setValue(LLSD((F64)volume.mHeightFalloff));
    mNoiseScaleSlider->setValue(LLSD((F64)volume.mNoiseScale));
    mNoiseSpeedSlider->setValue(LLSD((F64)volume.mNoiseSpeed));
    mTintSwatch->set(LLColor4(volume.mColor, 1.f), true, false);
    mDeleteButton->setEnabled(!volume.mLabel.empty() || volume.mEnabled);
    mDuplicateButton->setEnabled(!volume.mLabel.empty() || volume.mEnabled);
    mRefreshing = false;
}
