void FixturePanel::postBuild()
{
    mAlternate = getChild<LLView>("member_alternate");
}

void FixturePanel::refresh()
{
    setVisibleIfChanged(mAlternate, should_show);
}
