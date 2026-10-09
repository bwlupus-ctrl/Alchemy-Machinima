void Broken::draw() { if (mClock.due()) getChild<LLView>("slow"); getChild<LLView>("fast"); }
