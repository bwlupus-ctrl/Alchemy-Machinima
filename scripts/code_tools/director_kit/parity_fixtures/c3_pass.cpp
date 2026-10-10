// Parity fixture: lookups that v2_pass.xml satisfies (check 3). Not compiled.
void ALFixturePanel::postBuild()
{
    mSlider = getChild<LLSliderCtrl>("fx_slider");
    mCheck = findChild<LLCheckBoxCtrl>("fx_check");
    mCombo = getChild<LLComboBox>("fx_combo");
    mReadout = getChild<LLTextBox>("fx_readout");
    mReset = getChild<LLButton>("fx_combo_reset");
    mSide = getChild<LLSliderCtrl>("fx_" + side);
}
