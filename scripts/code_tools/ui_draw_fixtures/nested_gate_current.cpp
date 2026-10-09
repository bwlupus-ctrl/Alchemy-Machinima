void Current::draw()
{
    if (mVisible)
    {
        if (mClock.due())
        {
            getChild<LLView>("gated");
        }
    }
}
