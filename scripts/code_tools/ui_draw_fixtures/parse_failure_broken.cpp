void Valid::draw()
{
}

void Broken::draw()
{
    getChild<LLView>("unterminated");
