// Parity fixture: a literal absent from v2, and a pattern whose expansion fx_middle is absent (check 3). Not compiled.
void ALFixturePanel::postBuild()
{
    mGone = getChild<LLButton>("fx_nothere");
    mSide = getChild<LLSliderCtrl>("fx_" + side);
}
