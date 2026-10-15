# Director Settings value-wiring audit

The generated non-rail Settings page is opened by the top-bar gear and indexed by the kit search. All preferences persist in global settings. Both preference rows have bulk_reset=false, so page/card resets leave them alone.

| Control | Setting / type | Values / range | Handler | Read-back |
|---|---|---|---|---|
| settings_recovery_mode | DirectorRecoveryMode / S32, Persist 1, default 0 | 0 Ask me; 1 Restore automatically; 2 Don't restore (keep the backup) | ALKitSettingBinding typed write; offerSceneRecovery consumes it; recovery-service signal clears the old notification ignore when changed | ALKitSettingBinding setting signal refreshes combo |
| settings_restore_recovery | action | Restore last unsaved scene | bindDirectorSettingsPage -> restoreRecoveryScene -> loadScene("recovery", true), same path as prompt and automatic restore | refreshRecoveryControls enables only while recoveryPath exists; recovery idle refreshes it |
| settings_recovery_help | readout | Autosave and backup explanation | generated text | static text |
| settings_add_self_at_login | DirectorAddSelfAtLogin / Boolean, Persist 1, default 1 (existing setting verified) | false / true | ALKitSettingBinding typed write; existing login cast enrollment consumer | ALKitSettingBinding signal |
| settings_reset_prompts | action | Reset 'don't ask again' prompts | DirectorRecoveryMode=0; resets ignores only for notification template names beginning Director | combo setting signal; Director notification forms become unignored |
| settings_appearance_info | disabled readout | More options coming (e.g. menu colour scheme). | real Appearance card, disabled info row | static text |
| btn_director_settings | shell action | Settings page | ALKitShell -> selectPage("director_settings_tab") | selected gear state and Settings page title |
| DirectorRestoreRecovery ignore checkbox | DirectorRecoveryMode / S32 | unchecked: ordinary Restore/Discard; checked Restore -> 1, checked Discard -> 2 | standard save_option ignore remembers chosen button; onRecoveryResponse reads response["ignore"] and selected option | Settings combo signal; mode 2 leaves unresolved backup protected from autosave/shutdown |

The recovery notification uses the standard last-response ignore mechanism with an optional checkbox_label for the exact explanatory checkbox copy. Other notification labels retain their existing fallback. No other existing Director notification currently declares an ignore; the prefix-filtered reset also covers future Director ignores.
