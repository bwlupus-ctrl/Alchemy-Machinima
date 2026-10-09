void Broken::draw()
{
    if (!mChild)
        mChild = findChild<LLView>("name");
}
