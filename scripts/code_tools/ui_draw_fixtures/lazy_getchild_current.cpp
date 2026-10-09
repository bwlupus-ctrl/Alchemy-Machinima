void Current::draw()
{
    if (!mChild)
        mChild = getChild<LLView>("name");
}
