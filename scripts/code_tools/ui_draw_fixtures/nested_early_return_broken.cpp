void Broken::draw()
{
    if (mVisible)
    {
        if (!mClock.due()) return;
    }
    getChild<LLView>("not_actually_gated");
}
