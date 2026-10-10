// Parity fixture: LLSliderCtrl requested for fx_readout, which v2 builds as text (check 3 class mismatch). Not compiled.
void ALFixturePanel::postBuild()
{
    mReadout = getChild<LLSliderCtrl>("fx_readout");
}
