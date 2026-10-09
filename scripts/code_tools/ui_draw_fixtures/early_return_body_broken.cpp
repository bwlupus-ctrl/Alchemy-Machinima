void Broken::draw()
{
    if (mVisible) { mClock.due(); return; }
    getChild<LLView>("ungated");
}
