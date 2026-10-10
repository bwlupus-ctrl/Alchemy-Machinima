// Parity fixture: dynamic lookups no cpp_name_patterns.json template expands (check 3 unresolved). Not compiled.
void ALFixturePanel::postBuild()
{
    mA = getChild<LLSliderCtrl>("zz_" + side);
    mB = getChild<LLSliderCtrl>(name);
}
