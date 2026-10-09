void Current::draw()
{
    if (!mClock.due()) return;
    mText->setText("value");
}
