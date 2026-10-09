void Mixed::refresh()
{
    getChild<LLView>("reached_ungated");
}

void Mixed::draw()
{
    if (mClock.due())
    {
        refresh();
    }
    refresh();
}
