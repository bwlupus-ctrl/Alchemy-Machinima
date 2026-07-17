/**
* @file alviewermenu.cpp
* @brief Builds menus out of items. Imagine the fast, easy, fun Alchemy style
*
* $LicenseInfo:firstyear=2013&license=viewerlgpl$
* Copyright (C) 2013 Alchemy Developer Group
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
* $/LicenseInfo$
**/

#include "llviewerprecompiledheaders.h"
#include "alviewermenu.h"

// library
#include "llclipboard.h"
#include "llfloaterreg.h"
#include "llsdserialize.h"
#include "lltrans.h"
#include "llview.h"

// newview
#include "alavataractions.h"
#include "llcinematiccamera.h"  // [Cinematic] locked follow subject
#include "llactormover.h"       // [ActorMover] ghost locomotion subject
//#include "alcinematicmode.h"
#include "alderenderlist.h"
#include "alfloaterblocked.h"
#include "alfloaterparticleeditor.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llappviewer.h"
#include "llavatarpropertiesprocessor.h"
#include "llhudobject.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llselectmgr.h"
#include "lltexturecache.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "llvovolume.h"
#include "pipeline.h"

// llviewermenu.cpp
LLVOAvatar* find_avatar_from_object(LLViewerObject* object);

namespace
{
    bool enable_edit_particle_source()
    {
        LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
        for (LLObjectSelection::valid_root_iterator iter = selection->valid_root_begin();
            iter != selection->valid_root_end(); ++iter)
        {
            LLSelectNode* node = *iter;
            if (node->mPermissions->getOwner() == gAgent.getID())
            {
                return true;
            }
        }
        return false;
    }

    void edit_particle_source()
    {
        LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
        if (objectp)
        {
            ALFloaterParticleEditor* particleEditor = LLFloaterReg::showTypedInstance<ALFloaterParticleEditor>("particle_editor", LLSD(objectp->getID()), TAKE_FOCUS_YES);
            if (particleEditor)
                particleEditor->setObject(objectp);
        }
    }

    void world_clear_effects()
    {
        LLHUDObject::markViewerEffectsDead();
    }

    void world_sync_animations()
    {
        for (S32 i = 0; i < gObjectList.getNumObjects(); ++i)
        {
            LLViewerObject* object = gObjectList.getObject(i);
            if (object)
            {
                LLVOAvatar* avatarp = object->asAvatar();
                if (avatarp)
                {
                    for (const auto& playpair : avatarp->mPlayingAnimations)
                    {
                        avatarp->stopMotion(playpair.first, true);
                        avatarp->startMotion(playpair.first);
                    }
                }
            }
        }
    }

    void avatar_copy_data(const LLSD& userdata)
    {
        LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
        if (!objectp)
            return;

        LLVOAvatar* avatarp = find_avatar_from_object(objectp);
        if (avatarp)
        {
            ALAvatarActions::copyDataUI(avatarp->getID(), userdata);
        }
    }

    void avatar_undeform_self()
    {
        if (!isAgentAvatarValid())
            return;

        gAgentAvatarp->resetSkeleton(true);
        LLMessageSystem* msg = gMessageSystem;
        msg->newMessageFast(_PREHASH_AgentAnimation);
        msg->nextBlockFast(_PREHASH_AgentData);
        msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
        msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
        msg->nextBlockFast(_PREHASH_AnimationList);
        msg->addUUIDFast(_PREHASH_AnimID, LLUUID("e5afcabe-1601-934b-7e89-b0c78cac373a"));
        msg->addBOOLFast(_PREHASH_StartAnim, true);
        msg->nextBlockFast(_PREHASH_AnimationList);
        msg->addUUIDFast(_PREHASH_AnimID, LLUUID("d307c056-636e-dda6-4a3c-b3a43c431ca8"));
        msg->addBOOLFast(_PREHASH_StartAnim, true);
        msg->nextBlockFast(_PREHASH_AnimationList);
        msg->addUUIDFast(_PREHASH_AnimID, LLUUID("319b4e7a-18fc-1f9e-6411-dd10326c0c7e"));
        msg->addBOOLFast(_PREHASH_StartAnim, true);
        msg->nextBlockFast(_PREHASH_AnimationList);
        msg->addUUIDFast(_PREHASH_AnimID, LLUUID("f05d765d-0e01-5f9a-bfc2-fdc054757e55"));
        msg->addBOOLFast(_PREHASH_StartAnim, true);
        msg->nextBlockFast(_PREHASH_PhysicalAvatarEventList);
        msg->addBinaryDataFast(_PREHASH_TypeData, nullptr, 0);
        msg->sendReliable(gAgent.getRegion()->getHost());
    }

    void object_copy_key()
    {
        LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
        if (!objectp)
            return;

        const LLUUID& object_id = objectp->getID();
        LLWString idwstr = utf8string_to_wstring(object_id.asString());
        LLClipboard::instance().copyToClipboard(idwstr,0, narrow(idwstr.size()));
    }

    bool can_teleport_to()
    {
        LLVOAvatar* avatarp = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
        if (avatarp)
        {
            return ALAvatarActions::canTeleportTo(avatarp->getID());
        }
        return false;
    }

    void teleport_to()
    {
        LLVOAvatar* avatarp = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
        if (avatarp)
        {
            ALAvatarActions::teleportTo(avatarp->getID());
        }
    }

    bool can_manage_avatar_estate()
    {
        LLVOAvatar* avatarp = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
        if (avatarp)
        {
            return ALAvatarActions::canManageAvatarsEstate(avatarp->getID());
        }
        return false;
    }

    void manage_estate(const LLSD& param)
    {
        LLVOAvatar* avatarp = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
        if (avatarp)
        {
            S32 action = param.asInteger();
            switch (action)
            {
            case 0:
                ALAvatarActions::estateTeleportHome(avatarp->getID());
                break;
            case 1:
                ALAvatarActions::estateKick(avatarp->getID());
                break;
            case 2:
                ALAvatarActions::estateBan(avatarp->getID());
                break;
            }
        }
    }

    //void confirm_cinematic_mode(const LLSD& notification, const LLSD& response)
    //{
    //    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    //    if (option == 0) // OK
    //    {
    //        ALCinematicMode::toggle();
    //    }
    //}

    //bool toggle_cinematic_mode()
    //{
    //    LLNotification::Params params("CinematicConfirmHideUI");
    //    params.functor.function(boost::bind(&confirm_cinematic_mode, _1, _2));
    //    LLSD substitutions;
    //    substitutions["SHORTCUT"] = "Alt+Shift+C";
    //    params.substitutions = substitutions;
    //    if (!ALCinematicMode::isEnabled())
    //    {
    //        // hiding, so show notification
    //        LLNotifications::instance().add(params);
    //    }
    //    else
    //    {
    //        LLNotifications::instance().forceResponse(params, 0);
    //    }
    //    return true;
    //}

    bool get_saved_setting(const LLSD& userdata)
    {
        return gSavedSettings.getBOOL(userdata.asString());
    }

    bool is_powerful_wizard_object()
    {
        LLViewerObject* objpos = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();
        if (objpos)
        {
            if (objpos->permYouOwner() && gSavedSettings.getBOOL("AlchemyPowerfulWizard"))
                return true;
        }
        return false;
    }


    void object_explode()
    {
        LLViewerObject* objpos = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();
        if (objpos)
        {
            if (!objpos->permYouOwner())
            {
                LLNotificationsUtil::add("AlchemyUnpoweredWizard", LLSD());
                return;
            }

            LLNotificationsUtil::add("AlchemyExplosions", LLSD());

            /*
                NOTE: oh god how did this get here
            */
            LLSelectMgr::getInstance()->selectionUpdateTemporary(1);//set temp to TRUE
            LLSelectMgr::getInstance()->selectionUpdatePhysics(1);
            LLSelectMgr::getInstance()->sendDelink();
            LLSelectMgr::getInstance()->deselectAll();
        }
    }

    void object_destroy()
    {
        LLViewerObject* objpos = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();
        if (objpos)
        {
            if (!objpos->permYouOwner())
            {
                LLNotificationsUtil::add("AlchemyUnpoweredWizard", LLSD());
                return;
            }

            LLNotificationsUtil::add("AlchemyDestroyObject", LLSD());

            /*
                NOTE: Temporary objects, when thrown off world/put off world,
                do not report back to the viewer, nor go to lost and found.

                So we do selectionUpdateTemporary(1)
            */
            LLSelectMgr::getInstance()->selectionUpdateTemporary(1);//set temp to TRUE
            LLVector3 pos = objpos->getPosition();//get the x and the y
            pos.mV[VZ] = FLT_MAX;//create the z
            objpos->setPositionParent(pos);//set the x y z
            LLSelectMgr::getInstance()->sendMultipleUpdate(UPD_POSITION);//send the data
        }
    }

    void object_force_delete()
    {
        LLViewerObject* objpos = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject();
        if (objpos)
        {
            if (!objpos->permYouOwner())
            {
                LLNotificationsUtil::add("AlchemyUnpoweredWizard", LLSD());
                return;
            }
            LLSelectMgr::getInstance()->selectForceDelete();

        }
    }

    void spawn_debug_simfeatures()
    {
        if (LLViewerRegion* regionp = gAgent.getRegion())
        {
            LLSD sim_features, args;
            std::stringstream features_str;
            regionp->getSimulatorFeatures(sim_features);
            LLSDSerialize::toPrettyXML(sim_features, features_str);
            args["title"] = llformat("%s - %s", LLTrans::getString("SimulatorFeaturesTitle").c_str(), regionp->getName().c_str());
            args["data"] = features_str.str();
            LLFloaterReg::showInstance("generic_text", args);
        }
    }

    void destroy_texture(const LLUUID& id)
    {
        if (id.isNull() || id == IMG_DEFAULT
            || id == IMG_TRANSPARENT|| id == "8dcd4a48-2d37-4909-9f78-f7a9eb4ef903")
            return;
        LLViewerFetchedTexture* texture = LLViewerTextureManager::getFetchedTexture(id);
        if (texture)
            texture->clearFetchedResults();
        LLAppViewer::getTextureCache()->removeFromCache(id);
    }

    void object_texture_refresh()
    {
        for (LLObjectSelection::valid_iterator iter = LLSelectMgr::getInstance()->getSelection()->valid_begin();
             iter != LLSelectMgr::getInstance()->getSelection()->valid_end();
             ++iter)
        {
            LLSelectNode* node = *iter;
            if (!node)
                continue;
            std::map<LLUUID, std::vector<U8>> faces_per_tex;
            for (U8 i = 0; i < node->getObject()->getNumTEs(); ++i)
            {
                if (!node->isTESelected(i))
                continue;
                LLViewerTexture* img = node->getObject()->getTEImage(i);
                faces_per_tex[img->getID()].push_back(i);

                if (node->getObject()->getTE(i)->getMaterialParams().notNull())
                {
                LLViewerTexture* norm_img = node->getObject()->getTENormalMap(i);
                faces_per_tex[norm_img->getID()].push_back(i);
                LLViewerTexture* spec_img = node->getObject()->getTESpecularMap(i);
                faces_per_tex[spec_img->getID()].push_back(i);
                }

                LLPointer<LLGLTFMaterial> mat = node->getObject()->getTE(i)->getGLTFRenderMaterial();
                if (mat.notNull())
                {
                    for (U32 j = 0; j < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++j)
                    {
                        faces_per_tex[mat->mTextureId[j]].push_back(i);
                    }
                }
            }

            for (auto const& it : faces_per_tex)
            {
                destroy_texture(it.first);
            }

            if (node->getObject()->isSculpted() && !node->getObject()->isMesh())
            {
                const LLSculptParams* sculpt_params = (LLSculptParams*)node->getObject()->getSculptParams();
                if (sculpt_params)
                {
                LLUUID                  sculptie = sculpt_params->getSculptTexture();
                LLViewerFetchedTexture* tx       = LLViewerTextureManager::getFetchedTexture(sculptie);
                if (tx)
                {
                        const LLViewerTexture::ll_volume_list_t* pVolumeList = tx->getVolumeList(LLRender::SCULPT_TEX);
                        destroy_texture(sculptie);
                        for (S32 idxVolume = 0; idxVolume < tx->getNumVolumes(LLRender::SCULPT_TEX); ++idxVolume)
                        {
                            LLVOVolume* pVolume = pVolumeList->at(idxVolume);
                            if (pVolume)
                                pVolume->notifyMeshLoaded();
                        }
                }
                }
            }
        }
    }

    void avatar_texture_refresh()
    {
        LLVOAvatar* avatar = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
        if (!avatar) { return; }

        for (U32 baked_idx = 0; baked_idx < LLAvatarAppearanceDefines::BAKED_NUM_INDICES; ++baked_idx)
        {
            LLAvatarAppearanceDefines::ETextureIndex te_idx =
                LLAvatarAppearance::getDictionary()->bakedToLocalTextureIndex(
                    static_cast<LLAvatarAppearanceDefines::EBakedTextureIndex>(baked_idx));
            destroy_texture(avatar->getTE(te_idx)->getID());
        }
        LLAvatarPropertiesProcessor::getInstance()->sendAvatarTexturesRequest(avatar->getID());

        // *TODO: We want to refresh their attachments too!
    }

    class ALToggleLocationBar : public view_listener_t
    {
        bool handleEvent(const LLSD& userdata) override
        {
            const U32 val = userdata.asInteger();
            gSavedSettings.setU32("NavigationBarStyle", val);
            return true;
        }
    };

    class ALCheckLocationBar : public view_listener_t
    {
        bool handleEvent(const LLSD& userdata) override
        {
            return userdata.asInteger() == (S32)gSavedSettings.getU32("NavigationBarStyle");
        }
    };

    class LLCommunicateSetRejectTeleportOffers : public view_listener_t
    {
        bool handleEvent(const LLSD& userdata)
        {
            bool is_rejecting_offers = gSavedPerAccountSettings.getBOOL("ALRejectTeleportOffersMode");
            if (is_rejecting_offers)
            {
                gSavedPerAccountSettings.setBOOL("ALRejectTeleportOffersMode", false);
            }
            else
            {
                gSavedPerAccountSettings.setBOOL("ALRejectTeleportOffersMode", true);
                LLNotificationsUtil::add("RejectTeleportOffersModeSet");
            }
            return true;
        }
    };

    class LLCommunicateGetRejectTeleportOffers : public view_listener_t
    {
        bool handleEvent(const LLSD& userdata)
        {
            bool is_rejecting_offers = gSavedPerAccountSettings.getBOOL("ALRejectTeleportOffersMode");

            return is_rejecting_offers;
        }
    };

    class LLCommunicateSetRejectFriendshipRequests : public view_listener_t
    {
        bool handleEvent(const LLSD& userdata)
        {
            bool is_rejecting_offers = gSavedPerAccountSettings.getBOOL("ALRejectFriendshipRequestsMode");
            if (is_rejecting_offers)
            {
                gSavedPerAccountSettings.setBOOL("ALRejectFriendshipRequestsMode", false);
            }
            else
            {
                gSavedPerAccountSettings.setBOOL("ALRejectFriendshipRequestsMode", true);
                LLNotificationsUtil::add("RejectFriendshipRequestsModeSet");
            }
            return true;
        }
    };

    class LLCommunicateGetRejectFriendshipRequests : public view_listener_t
    {
        bool handleEvent(const LLSD& userdata)
        {
            bool is_rejecting_offers = gSavedPerAccountSettings.getBOOL("ALRejectFriendshipRequestsMode");

            return is_rejecting_offers;
        }
    };

    class LLCommunicateSetAutoRespond : public view_listener_t
    {
        bool handleEvent(const LLSD& userdata)
        {
            bool is_autorespond_set = gSavedPerAccountSettings.getBOOL("AlchemyAutoresponseEnable");
            if (is_autorespond_set)
            {
                gSavedPerAccountSettings.setBOOL("AlchemyAutoresponseEnable", false);
            }
            else
            {
                gSavedPerAccountSettings.setBOOL("AlchemyAutoresponseEnable", true);
                LLNotificationsUtil::add("AutoRespondModeSet");
            }
            return true;
        }
    };

    class LLCommunicateCheckAutoRespond : public view_listener_t
    {
        bool handleEvent(const LLSD& userdata)
        {
            bool is_autorespond_set = gSavedPerAccountSettings.getBOOL("AlchemyAutoresponseEnable");
            return is_autorespond_set;
        }
    };

    class LLCommunicateSetAutoRespondNonFriends : public view_listener_t
    {
        bool handleEvent(const LLSD& userdata)
        {
            bool is_autorespond_nonfriends_set = gSavedPerAccountSettings.getBOOL("AlchemyAutoresponseNotFriendEnable");
            if (is_autorespond_nonfriends_set)
            {
                gSavedPerAccountSettings.setBOOL("AlchemyAutoresponseNotFriendEnable", false);
            }
            else
            {
                gSavedPerAccountSettings.setBOOL("AlchemyAutoresponseNotFriendEnable", true);
                LLNotificationsUtil::add("AutoRespondNonFriendsModeSet");
            }
            return true;
        }
    };

    class LLCommunicateCheckAutoRespondNonFriends : public view_listener_t
    {
        bool handleEvent(const LLSD& userdata)
        {
            bool is_autorespond_nonfriends_set = gSavedPerAccountSettings.getBOOL("AlchemyAutoresponseNotFriendEnable");
            return is_autorespond_nonfriends_set;
        }
    };

// [SL:KB] - Patch: World-Derender | Checked: 2012-06-08 (Catznip-3.3)
    void handle_view_blocked(const LLSD& sdParam)
    {
        if (LLVOAvatar* pAvatar = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject()))
        {
            std::string strParam = sdParam.asString();
            if (BLOCKED_TAB_NAME == strParam)
                strParam = BLOCKED_PARAM_NAME;
            else if (DERENDER_TAB_NAME == strParam)
                strParam = DERENDER_PARAM_NAME;
            else if (EXCEPTION_TAB_NAME == strParam)
                strParam = EXCEPTION_PARAM_NAME;

            LLFloaterReg::showInstance("blocked", LLSD().with(strParam, pAvatar->getID()));
        }
        else
        {
            LLFloaterReg::showInstance("blocked", sdParam);
        }
    }

    void handle_object_derender(const LLSD& sdParam)
    {
        std::vector<LLUUID> idList;
        if (ALDerenderList::instance().addSelection("persistent" == sdParam.asString(), &idList))
        {
            LLFloaterReg::showInstance("blocked", LLSD().with("derender_to_select", idList.front()));
        }
    }

    bool enable_object_derender()
    {
        return ALDerenderList::canAddSelection();
    }
// [/SL:KB]

// [BDMerge G3.3 Phase 2] Right-click "Volumetric Shaft" toggle. Session-only
// per-projector opt-in for the volumetric light-cone effect; mirrors the
// derender selection plumbing but stores UUIDs in gPipeline's in-memory set
// (never persisted). Toggles every root object in the current selection.
    LLVOVolume* get_selected_projector_volume()
    {
        // A spotlight projector is often a CHILD prim of a linked fixture, not the
        // root, while the primary selected object is usually the root - so checking
        // ONLY the primary object wrongly greyed this menu for linksets (it worked
        // only on UNLINKED lights). Scan every selected prim AND each one's linkset
        // children for the first spotlight. The render side already matches a flagged
        // root id against a child light's root-edit id, so toggling the root (in
        // handle_object_volumetric_shaft) lights the whole fixture's projectors.
        LLObjectSelectionHandle hSel = LLSelectMgr::getInstance()->getSelection();
        if (hSel.isNull())
            return nullptr;

        // [BDMerge fix] Iterate the RAW selection (begin/end = object-non-null only),
        // NOT valid_begin/valid_end. `mValid` is set only after the server's
        // ObjectProperties reply arrives, which for a right-click selection of another
        // person's NO-MOD object typically never comes - so valid_* was empty and this
        // enable gate greyed the item for any light that isn't yours (reported
        // in-world). Volumetric Shaft / Hero Beam are purely CLIENT-SIDE, session-only
        // render toggles keyed by UUID; they touch nothing on the object and need no
        // properties/permissions - only that it renders as a spotlight (client data
        // available for everything you can see). Ownership/mod is irrelevant.
        for (LLObjectSelection::iterator itObj = hSel->begin(), endObj = hSel->end();
             itObj != endObj; ++itObj)
        {
            const LLSelectNode* pNode = *itObj;
            LLViewerObject* pObj = (pNode) ? pNode->getObject() : nullptr;
            if (!pObj)
                continue;

            LLVOVolume* pVol = dynamic_cast<LLVOVolume*>(pObj);
            if (pVol && pVol->isLightSpotlight())
                return pVol;

            // Children too, in case only the root node is in the selection.
            for (const LLPointer<LLViewerObject>& pChildPtr : pObj->getChildren())
            {
                LLVOVolume* pChildVol = dynamic_cast<LLVOVolume*>(pChildPtr.get());
                if (pChildVol && pChildVol->isLightSpotlight())
                    return pChildVol;
            }
        }
        return nullptr;
    }

    void handle_object_volumetric_shaft(const LLSD& /*sdParam*/)
    {
        LLObjectSelectionHandle hSel = LLSelectMgr::getInstance()->getSelection();
        if (hSel.isNull())
            return;

        for (LLObjectSelection::root_iterator itObj = hSel->root_begin(), endObj = hSel->root_end();
             itObj != endObj; ++itObj)
        {
            const LLSelectNode* pNode = *itObj;
            LLViewerObject* pObj = (pNode) ? pNode->getObject() : nullptr;
            if (pObj && pObj->getID().notNull())
                LLPipeline::toggleVolumetricShaft(pObj->getID());
        }
    }

    bool enable_object_volumetric_shaft()
    {
        // Only meaningful on spotlight projectors (the only lights that can cast
        // the shadow slots the effect marches). No-op harmlessly otherwise.
        return get_selected_projector_volume() != nullptr;
    }

    bool check_object_volumetric_shaft()
    {
        LLViewerObject* pObj = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
        return pObj && LLPipeline::isVolumetricShaftEnabled(pObj->getID());
    }

    // [BDMerge G3.3 Batch 3] Right-click "Cast Shadows" toggle. Session-only
    // per-projector opt-OUT of shadow-slot eligibility: an unchecked projector
    // still lights the scene but casts no shadow and frees its slot. Default
    // (not in the set) == casts shadows. Toggles every root in the selection.
    void handle_object_cast_shadows(const LLSD& /*sdParam*/)
    {
        LLObjectSelectionHandle hSel = LLSelectMgr::getInstance()->getSelection();
        if (hSel.isNull())
            return;

        for (LLObjectSelection::root_iterator itObj = hSel->root_begin(), endObj = hSel->root_end();
             itObj != endObj; ++itObj)
        {
            const LLSelectNode* pNode = *itObj;
            LLViewerObject* pObj = (pNode) ? pNode->getObject() : nullptr;
            if (pObj && pObj->getID().notNull())
                LLPipeline::toggleProjectorCastShadows(pObj->getID());
        }
    }

    bool enable_object_cast_shadows()
    {
        // Only meaningful on spotlight projectors (the only lights that cast
        // shadow slots). No-op harmlessly otherwise.
        return get_selected_projector_volume() != nullptr;
    }

    bool check_object_cast_shadows()
    {
        // Checked when the projector DOES cast shadows (i.e. NOT in the opt-out
        // set), so the default state shows as ticked.
        LLViewerObject* pObj = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
        return pObj && !LLPipeline::isProjectorNoShadow(pObj->getID());
    }

    // [BDMerge F4] Right-click "Hero Beam" toggle. Session-only per-projector flag:
    // a hero projector is excluded from froxel light injection and marches its sharp
    // per-cone shaft on top of the froxel atmosphere (film key-light-sharp/air-soft
    // split). With froxel OFF the flag is inert. Toggles every root in the selection,
    // mirroring the Volumetric Shaft / Cast Shadows toggles.
    void handle_object_hero_beam(const LLSD& /*sdParam*/)
    {
        LLObjectSelectionHandle hSel = LLSelectMgr::getInstance()->getSelection();
        if (hSel.isNull())
            return;

        for (LLObjectSelection::root_iterator itObj = hSel->root_begin(), endObj = hSel->root_end();
             itObj != endObj; ++itObj)
        {
            const LLSelectNode* pNode = *itObj;
            LLViewerObject* pObj = (pNode) ? pNode->getObject() : nullptr;
            if (pObj && pObj->getID().notNull())
            {
                LLPipeline::toggleHeroProjector(pObj->getID());
                // [F4 debug] Decisive breadcrumb for "Hero Beam does nothing" reports:
                // confirms the click reached the handler and which id carries the flag.
                LL_INFOS("HeroBeam") << "toggled " << pObj->getID() << " -> "
                                     << (LLPipeline::isHeroProjector(pObj->getID()) ? "ON" : "OFF") << LL_ENDL;
            }
        }
    }

    bool enable_object_hero_beam()
    {
        // Only meaningful on spotlight projectors (the shaft the hero beam marches).
        // Same enable condition as the Volumetric Shaft item. No-op harmlessly
        // otherwise, and inert unless the shaft is also flagged + froxel is on.
        return get_selected_projector_volume() != nullptr;
    }

    bool check_object_hero_beam()
    {
        // [F4 fix] Reflect the flag whether it was set on this prim or its root
        // (the toggle iterates selection roots; Edit Linked makes a child its own
        // selection root, so both ids are legitimate carriers of the flag).
        LLViewerObject* pObj = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
        if (!pObj)
            return false;
        if (LLPipeline::isHeroProjector(pObj->getID()))
            return true;
        LLViewerObject* pRoot = pObj->getRootEdit();
        return pRoot && LLPipeline::isHeroProjector(pRoot->getID());
    }

    // [BDMerge G3.3 Batch 1 C] Snapshot the current global shaft sliders into a
    // per-projector override for every selected root, and flag it on (capture
    // implies enable). Session-only; cleared on relog with the flag set.
    void handle_object_shaft_capture(const LLSD& /*sdParam*/)
    {
        LLPipeline::VolumetricShaftOverride ov;
        ov.multiplier   = gSavedSettings.getF32("BDMergeProjectorVolumetricsMultiplier");
        ov.feather      = gSavedSettings.getF32("BDMergeProjectorVolumetricsFeather");
        ov.anisotropy   = gSavedSettings.getF32("BDMergeProjectorVolumetricsAnisotropy");
        ov.density      = gSavedSettings.getF32("BDMergeProjectorVolumetricsDensity");
        ov.tint         = gSavedSettings.getColor3("BDMergeProjectorVolumetricsTint");
        ov.tintStrength = gSavedSettings.getF32("BDMergeProjectorVolumetricsTintStrength");
        // [BDMerge F4] snapshot the rim art-direction levers too (RimThreshold stays global)
        ov.rimStrength  = gSavedSettings.getF32("BDMergeProjectorVolumetricsRimStrength");
        ov.rimPower     = gSavedSettings.getF32("BDMergeProjectorVolumetricsRimPower");
        ov.rimWrap      = gSavedSettings.getF32("BDMergeProjectorVolumetricsRimWrap");
        ov.rimSoftness  = gSavedSettings.getF32("BDMergeProjectorVolumetricsRimSoftness");

        LLObjectSelectionHandle hSel = LLSelectMgr::getInstance()->getSelection();
        if (hSel.isNull())
            return;

        for (LLObjectSelection::root_iterator itObj = hSel->root_begin(), endObj = hSel->root_end();
             itObj != endObj; ++itObj)
        {
            const LLSelectNode* pNode = *itObj;
            LLViewerObject* pObj = (pNode) ? pNode->getObject() : nullptr;
            if (pObj && pObj->getID().notNull())
                LLPipeline::setVolumetricShaftOverride(pObj->getID(), ov);
        }
    }

    // [BDMerge G3.3 Batch 1 C] Revert every selected root to the global sliders.
    void handle_object_shaft_clear_override(const LLSD& /*sdParam*/)
    {
        LLObjectSelectionHandle hSel = LLSelectMgr::getInstance()->getSelection();
        if (hSel.isNull())
            return;

        for (LLObjectSelection::root_iterator itObj = hSel->root_begin(), endObj = hSel->root_end();
             itObj != endObj; ++itObj)
        {
            const LLSelectNode* pNode = *itObj;
            LLViewerObject* pObj = (pNode) ? pNode->getObject() : nullptr;
            if (pObj && pObj->getID().notNull())
                LLPipeline::clearVolumetricShaftOverride(pObj->getID());
        }
    }

    bool enable_object_shaft_clear_override()
    {
        LLViewerObject* pObj = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
        return pObj && LLPipeline::hasVolumetricShaftOverride(pObj->getID());
    }

    // [BDMerge G2.3 per-target] Right-click "Alpha Mode" submenu. Session-only
    // per-object override of alpha handling (0=Default, 1=Force Mask, 2=Force Blend),
    // keyed by object ROOT id. Purely client-side render state - works on ANY owner /
    // no-mod content (no permission/mValid gate), mirroring the Volumetric Shaft /
    // Hero Beam session toggles. setAlphaModeOverride() re-routes the geometry
    // immediately, so the change is visible on click.
    void handle_object_alpha_mode(const LLSD& sdParam)
    {
        const S32 mode = sdParam.asInteger(); // 0/1/2 from the menu param
        LLObjectSelectionHandle hSel = LLSelectMgr::getInstance()->getSelection();
        if (hSel.isNull())
            return;

        // Iterate the RAW selection roots (root_begin/root_end = object-non-null, NOT
        // valid_root_*). mValid needs a server ObjectProperties reply that never comes
        // for others' no-mod objects, so a valid_* gate would silently skip them.
        for (LLObjectSelection::root_iterator itObj = hSel->root_begin(), endObj = hSel->root_end();
             itObj != endObj; ++itObj)
        {
            const LLSelectNode* pNode = *itObj;
            LLViewerObject* pObj = (pNode) ? pNode->getObject() : nullptr;
            if (pObj && pObj->getID().notNull())
                LLPipeline::setAlphaModeOverride(pObj->getID(), mode); // rebuild happens inside
        }
    }

    bool check_object_alpha_mode(const LLSD& sdParam)
    {
        const S32 mode = sdParam.asInteger();
        LLViewerObject* pObj = LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
        if (!pObj)
            return false;
        LLUUID objId = pObj->getRootEdit() ? pObj->getRootEdit()->getID() : LLUUID::null;
        LLUUID avId  = pObj->getAvatar() ? pObj->getAvatar()->getID() : LLUUID::null;
        return LLPipeline::resolveAlphaMode(objId, avId) == mode;
    }

    bool enable_object_alpha_mode()
    {
        // Enabled whenever the selection has any volume object - no permission gate.
        LLObjectSelectionHandle hSel = LLSelectMgr::getInstance()->getSelection();
        if (hSel.isNull())
            return false;
        for (LLObjectSelection::iterator itObj = hSel->begin(), endObj = hSel->end();
             itObj != endObj; ++itObj)
        {
            const LLSelectNode* pNode = *itObj;
            LLViewerObject* pObj = (pNode) ? pNode->getObject() : nullptr;
            if (dynamic_cast<LLVOVolume*>(pObj))
                return true;
        }
        return false;
    }

    // [BDMerge G2.3 per-target] Right-click avatar "Attachments Alpha" submenu.
    // Applies the same session-only alpha-mode override to the AVATAR's id, so ALL of
    // that avatar's attachments follow (resolveAlphaMode falls back to the avatar id
    // when no per-object override is set). Keyed off the right-clicked avatar, resolved
    // the same way every other Avatar.* handler does (find_avatar_from_object on the
    // primary selected object). Object-specific overrides still beat this avatar-wide one.
    void handle_avatar_alpha_mode(const LLSD& sdParam)
    {
        const S32 mode = sdParam.asInteger();
        LLVOAvatar* avatarp = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
        if (avatarp && avatarp->getID().notNull())
            LLPipeline::setAlphaModeOverride(avatarp->getID(), mode); // rebuilds all attachments inside
    }

    bool check_avatar_alpha_mode(const LLSD& sdParam)
    {
        const S32 mode = sdParam.asInteger();
        LLVOAvatar* avatarp = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
        if (!avatarp)
            return false;
        return LLPipeline::getAlphaModeOverride(avatarp->getID()) == mode;
    }

    bool enable_avatar_alpha_mode()
    {
        return find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject()) != nullptr;
    }

// [Cinematic] right-click avatar > lock as the Cinematic Camera follow subject
// (session-only; clicking the same avatar again clears the lock)
    void handle_avatar_cinecam_follow(const LLSD&)
    {
        LLVOAvatar* avatarp = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
        if (avatarp && avatarp->getID().notNull())
            LLCinematicCamera::toggleFollowTarget(avatarp->getID());
    }

    bool check_avatar_cinecam_follow(const LLSD&)
    {
        LLVOAvatar* avatarp = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
        return avatarp && LLCinematicCamera::isFollowTarget(avatarp->getID());
    }

// [ActorMover] right-click avatar (or animesh, resolved to its control avatar
// by find_avatar_from_object) > toggle ghost-locomotion roster membership
    void handle_avatar_actor_mover_target(const LLSD&)
    {
        LLVOAvatar* avatarp = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
        if (avatarp && avatarp->getID().notNull())
            LLActorMover::toggleTarget(avatarp->getID());
    }

    bool check_avatar_actor_mover_target(const LLSD&)
    {
        LLVOAvatar* avatarp = find_avatar_from_object(LLSelectMgr::getInstance()->getSelection()->getPrimaryObject());
        return avatarp && LLActorMover::isTarget(avatarp->getID());
    }
}

////////////////////////////////////////////////////////

void ALViewerMenu::initialize_menus()
{
    LLUICtrl::EnableCallbackRegistry::Registrar& enable = LLUICtrl::EnableCallbackRegistry::currentRegistrar();
    enable.add("Alchemy.PowerfulWizardObject", [](LLUICtrl* ctrl, const LLSD& param) { return is_powerful_wizard_object(); });
    enable.add("Avatar.EnableManageEstate", [](LLUICtrl* ctrl, const LLSD& param) { return can_manage_avatar_estate(); });
    enable.add("Avatar.EnableTeleportTo", [](LLUICtrl* ctrl, const LLSD& param) { return can_teleport_to(); });
    enable.add("Object.EnableEditParticles", [](LLUICtrl* ctrl, const LLSD& param) { return enable_edit_particle_source(); });
    enable.add("SavedSetting", [](LLUICtrl* ctrl, const LLSD& param) { return get_saved_setting(param); });

    LLUICtrl::CommitCallbackRegistry::Registrar& commit = LLUICtrl::CommitCallbackRegistry::currentRegistrar();
    commit.add("Avatar.CopyData",       [](LLUICtrl* ctrl, const LLSD& param) { avatar_copy_data(param); });
    commit.add("Avatar.ManageEstate", [](LLUICtrl* ctrl, const LLSD& param) { manage_estate(param); });
    commit.add("Avatar.TeleportTo", [](LLUICtrl* ctrl, const LLSD& param) { teleport_to(); });
    commit.add("Avatar.RefreshTexture", [](LLUICtrl* ctrl, const LLSD& param) { avatar_texture_refresh(); });

    commit.add("Advanced.DebugSimFeatures", [](LLUICtrl* ctrl, const LLSD& param) { spawn_debug_simfeatures(); });

    commit.add("Camera.SavePosition", [](LLUICtrl* ctrl, const LLSD& param) { gAgentCamera.storeCameraPosition(); });
    commit.add("Camera.RestorePosition", [](LLUICtrl* ctrl, const LLSD& param) { gAgentCamera.loadCameraPosition(); });

    view_listener_t::addMenu(new LLCommunicateSetRejectTeleportOffers(), "Communicate.SetRejectTeleportOffers");
    view_listener_t::addMenu(new LLCommunicateGetRejectTeleportOffers(), "Communicate.GetRejectTeleportOffers");
    view_listener_t::addMenu(new LLCommunicateSetRejectFriendshipRequests(), "Communicate.SetRejectFriendshipRequests");
    view_listener_t::addMenu(new LLCommunicateGetRejectFriendshipRequests(), "Communicate.GetRejectFriendshipRequests");
    view_listener_t::addMenu(new LLCommunicateSetAutoRespond(), "Communicate.SetAutoRespond");
    view_listener_t::addMenu(new LLCommunicateSetAutoRespondNonFriends(), "Communicate.SetAutoRespondNonFriends");
    view_listener_t::addMenu(new LLCommunicateCheckAutoRespond(), "Communicate.GetAutoRespond");
    view_listener_t::addMenu(new LLCommunicateCheckAutoRespondNonFriends(), "Communicate.GetAutoRespondNonFriends");

    commit.add("Object.CopyID", [](LLUICtrl* ctrl, const LLSD& param) { object_copy_key(); });
    commit.add("Object.EditParticles",  [](LLUICtrl* ctrl, const LLSD& param) { edit_particle_source(); });
    commit.add("Object.AlchemyExplode", [](LLUICtrl* ctrl, const LLSD& param) { object_explode(); });
    commit.add("Object.AlchemyDestroy", [](LLUICtrl* ctrl, const LLSD& param) { object_destroy(); });
    commit.add("Object.AlchemyForceDelete", [](LLUICtrl* ctrl, const LLSD& param) { object_force_delete(); });
    commit.add("Object.RefreshTexture", [](LLUICtrl* ctrl, const LLSD& param) { object_texture_refresh(); });

// [SL:KB] - Patch: World-Derender | Checked: 2011-12-15 (Catznip-3.2)
    commit.add("Object.Derender", boost::bind(&handle_object_derender, _2));
    enable.add("Object.EnableDerender", boost::bind(&enable_object_derender));
    // [/SL:KB]

// [BDMerge G3.3 Phase 2] session-only per-projector volumetric shaft toggle
    commit.add("Object.VolumetricShaft", boost::bind(&handle_object_volumetric_shaft, _2));
    enable.add("Object.EnableVolumetricShaft", boost::bind(&enable_object_volumetric_shaft));
    enable.add("Object.CheckVolumetricShaft", boost::bind(&check_object_volumetric_shaft));
// [BDMerge G3.3 Batch 3] session-only per-projector cast-shadows opt-out toggle
    commit.add("Object.CastShadows", boost::bind(&handle_object_cast_shadows, _2));
    enable.add("Object.EnableCastShadows", boost::bind(&enable_object_cast_shadows));
    enable.add("Object.CheckCastShadows", boost::bind(&check_object_cast_shadows));
// [BDMerge F4] session-only per-projector Hero Beam toggle (per-cone march over froxel)
    commit.add("Object.HeroBeam", boost::bind(&handle_object_hero_beam, _2));
    enable.add("Object.EnableHeroBeam", boost::bind(&enable_object_hero_beam));
    enable.add("Object.CheckHeroBeam", boost::bind(&check_object_hero_beam));
// [BDMerge G3.3 Batch 1 C] per-projector volumetric override capture/clear
    commit.add("Object.ShaftCaptureOverride", boost::bind(&handle_object_shaft_capture, _2));
    enable.add("Object.EnableShaftCaptureOverride", boost::bind(&enable_object_volumetric_shaft));
    commit.add("Object.ShaftClearOverride", boost::bind(&handle_object_shaft_clear_override, _2));
    enable.add("Object.EnableShaftClearOverride", boost::bind(&enable_object_shaft_clear_override));
// [BDMerge G2.3 per-target] session-only per-object / per-avatar alpha-mode override
    commit.add("Object.AlphaMode", boost::bind(&handle_object_alpha_mode, _2));
    enable.add("Object.CheckAlphaMode", boost::bind(&check_object_alpha_mode, _2));
    enable.add("Object.EnableAlphaMode", boost::bind(&enable_object_alpha_mode));
    commit.add("Avatar.AlphaMode", boost::bind(&handle_avatar_alpha_mode, _2));
    enable.add("Avatar.CheckAlphaMode", boost::bind(&check_avatar_alpha_mode, _2));
    enable.add("Avatar.EnableAlphaMode", boost::bind(&enable_avatar_alpha_mode));
    // [Cinematic] locked follow subject (reuses the alpha-mode avatar resolution + enable)
    commit.add("Avatar.CineCamFollow", boost::bind(&handle_avatar_cinecam_follow, _2));
    enable.add("Avatar.CheckCineCamFollow", boost::bind(&check_avatar_cinecam_follow, _2));
    // [ActorMover] ghost-locomotion subject
    commit.add("Avatar.ActorMoverTarget", boost::bind(&handle_avatar_actor_mover_target, _2));
    enable.add("Avatar.CheckActorMoverTarget", boost::bind(&check_avatar_actor_mover_target, _2));

    // [SL:KB] - Patch: World-RenderExceptions | Checked: Catznip-5.2
    commit.add("View.Blocked", boost::bind(&handle_view_blocked, _2));
    // [/SL:KB]

    commit.add("Tools.UndeformSelf", [](LLUICtrl* ctrl, const LLSD& param) { avatar_undeform_self(); });

    commit.add("World.ClearEffects",    [](LLUICtrl* ctrl, const LLSD& param) { world_clear_effects(); });
    commit.add("World.SyncAnimations",  [](LLUICtrl* ctrl, const LLSD& param) { world_sync_animations(); });

    //commit.add("View.ToggleCinematicMode", [](LLUICtrl* ctrl, const LLSD& param) { toggle_cinematic_mode(); });

    view_listener_t::addMenu(new ALToggleLocationBar(), "ToggleLocationBar");
    view_listener_t::addMenu(new ALCheckLocationBar(), "CheckLocationBar");
}
