void Broken::draw()
{
    LLControlVariable* control = mControl;
    if (mClock.due())
    {
        control->setValue(true);
    }
}
